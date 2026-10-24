#include "ecusimulator.h"
#include "decoding.h"
#include "diagdefaults.h"
#include "udpbus.h"

#include <QRandomGenerator>
#include <QTimer>

#include <cmath>

namespace {

QByteArray negative(quint8 serviceId, quint8 nrc)
{
    return bytesOf({0x7F, serviceId, nrc});
}

const int ResetTime = 500;     // ms the ECU is silent after an ECUReset
const int LockoutTime = 10000; // ms seeds are refused after too many wrong keys
const int SlowWriteTime = 1500; // ms a write takes in the programming session

} // namespace

EcuSimulator::EcuSimulator(QObject *parent)
    : QObject(parent)
    , m_bus(new UdpBus(this))
    , m_s3Timer(new QTimer(this))
    , m_busTimer(new QTimer(this))
{
    connect(m_bus, &UdpBus::received, this, &EcuSimulator::receive);
    m_s3Timer->setSingleShot(true);
    m_s3Timer->setInterval(S3Time);
    connect(m_s3Timer, &QTimer::timeout, this, &EcuSimulator::enterDefaultSession);
    m_busTimer->setInterval(5);
    connect(m_busTimer, &QTimer::timeout, this, &EcuSimulator::sendBusTraffic);
}

bool EcuSimulator::start(const usc::Database *description, const ff::Database *database, quint16 port,
                         const QHostAddress &testerAddress, quint16 testerPort, int idSize, QString *errorMessage)
{
    stop();
    if (!m_bus->open(QHostAddress::AnyIPv4, port, testerAddress, testerPort, idSize, errorMessage)) {
        *errorMessage = "Simulated ECU: " + *errorMessage;
        return false;
    }
    m_description = description;
    m_database = database;
    m_clock.start();
    m_silentUntil = 0;
    m_failedAttempts.clear();
    m_lockedOutUntil.clear();
    m_dataRecords.clear();
    if (m_description) {
        m_requestId = m_description->network().requestId;
        m_responseId = m_description->network().responseId;
        for (const usc::DataIdentifier &did : m_description->allDataIdentifiers()) {
            QByteArray record(did.length, '\0');
            for (const usc::DataTypeMapping &mapping : did.mappings)
                encodeInitialValue(*m_description, mapping, did.name, &record);
            m_dataRecords.insert(did.did, record);
        }
        enterDefaultSession();
    }
    m_nextTransmit.clear();
    m_busTimer->start();
    return true;
}

void EcuSimulator::stop()
{
    m_busTimer->stop();
    m_s3Timer->stop();
    m_bus->close();
    m_description = nullptr;
    m_database = nullptr;
}

void EcuSimulator::receive(quint32 id, const QByteArray &payload)
{
    if (m_description && id == m_requestId && !payload.isEmpty())
        handleRequest(payload);
}

void EcuSimulator::handleRequest(const QByteArray &request)
{
    if (m_clock.elapsed() < m_silentUntil)
        return;
    m_s3Timer->start(); // any request keeps a non-default session alive

    const quint8 serviceId = quint8(request[0]);
    if (serviceId == 0x2E && request.size() >= 3) {
        const usc::Session *session = m_description->findSession(m_sessionId);
        const QByteArray response = respond(request);
        if (session && session->sessionType == 0x02 && !isNegativeResponse(response)) {
            // Writing flash memory takes a while: ask the tester to wait (P2*), then answer.
            sendResponse(negative(serviceId, 0x78));
            QTimer::singleShot(SlowWriteTime, this, [this, response] { sendResponse(response); });
            return;
        }
        sendResponse(response);
        return;
    }

    const QByteArray response = respond(request);
    // suppressPosRspMsgIndicationBit: a positive response is not sent.
    const bool hasSubFunction = serviceId == 0x10 || serviceId == 0x11 || serviceId == 0x27 || serviceId == 0x3E;
    const bool suppress = hasSubFunction && request.size() >= 2 && (quint8(request[1]) & 0x80);
    if (!response.isEmpty() && !(suppress && !isNegativeResponse(response)))
        sendResponse(response);

    if (serviceId == 0x11 && !isNegativeResponse(response)) {
        // Reset: back to the default session, locked, and silent for a moment.
        enterDefaultSession();
        m_silentUntil = m_clock.elapsed() + ResetTime;
    }
}

QByteArray EcuSimulator::respond(const QByteArray &request)
{
    switch (quint8(request[0])) {
    case 0x10: return diagnosticSessionControl(request);
    case 0x11: return ecuReset(request);
    case 0x22: return readDataByIdentifier(request);
    case 0x27: return securityAccess(request);
    case 0x2E: return writeDataByIdentifier(request);
    case 0x3E: return testerPresent(request);
    }
    return negative(quint8(request[0]), 0x11); // serviceNotSupported
}

QByteArray EcuSimulator::diagnosticSessionControl(const QByteArray &request)
{
    if (request.size() != 2)
        return negative(0x10, 0x13);
    const quint8 sessionType = quint8(request[1]) & 0x7F;
    const usc::Session *session = findSessionByType(*m_description, sessionType);
    if (!session)
        return negative(0x10, 0x12);
    m_sessionId = session->id;
    m_unlockedLevelIds.clear(); // a session change locks the ECU again
    m_seeds.clear();
    const int p2StarSteps = session->p2Star / usc::P2StarStep;
    return bytesOf({0x50, sessionType, session->p2 >> 8, session->p2 & 0xFF,
                       p2StarSteps >> 8, p2StarSteps & 0xFF});
}

QByteArray EcuSimulator::ecuReset(const QByteArray &request)
{
    if (request.size() != 2)
        return negative(0x11, 0x13);
    const quint8 resetType = quint8(request[1]) & 0x7F;
    const usc::ResetType *reset = findResetTypeByValue(*m_description, resetType);
    if (!reset)
        return negative(0x11, 0x12);
    if (!inSession(reset->sessionIds))
        return negative(0x11, 0x7E);
    if (!isUnlocked(reset->securityLevelIds))
        return negative(0x11, 0x33);
    QByteArray response = bytesOf({0x51, resetType});
    if (resetType == usc::EnableRapidPowerShutDown)
        response.append(char(reset->powerDownTime));
    return response;
}

QByteArray EcuSimulator::readDataByIdentifier(const QByteArray &request)
{
    if (request.size() != 3)
        return negative(0x22, 0x13);
    const quint16 didNumber = quint16((quint8(request[1]) << 8) | quint8(request[2]));
    const usc::DataIdentifier *did = findDid(*m_description, didNumber);
    if (!did || !usc::isReadable(*did) || !inSession(did->readSessionIds))
        return negative(0x22, 0x31);
    if (!isUnlocked(did->readSecurityLevelIds))
        return negative(0x22, 0x33);
    return bytesOf({0x62, quint8(request[1]), quint8(request[2])}) + m_dataRecords.value(didNumber);
}

QByteArray EcuSimulator::securityAccess(const QByteArray &request)
{
    if (request.size() < 2)
        return negative(0x27, 0x13);
    const quint8 subFunction = quint8(request[1]) & 0x7F;
    const bool isSeedRequest = subFunction % 2 == 1;
    const usc::SecurityLevel *level =
        findLevelByRequestSeed(*m_description, isSeedRequest ? subFunction : subFunction - 1);
    if (!level)
        return negative(0x27, 0x12);
    if (!inSession(level->sessionIds))
        return negative(0x27, 0x7E);

    if (isSeedRequest) {
        if (request.size() != 2)
            return negative(0x27, 0x13);
        if (m_clock.elapsed() < m_lockedOutUntil.value(level->id))
            return negative(0x27, 0x37);
        QByteArray seed(level->seedLength, '\0');
        if (!m_unlockedLevelIds.contains(level->id)) {
            // All zero means "already unlocked", so a real seed is never all zero.
            for (int i = 0; i < seed.size(); ++i)
                seed[i] = char(QRandomGenerator::global()->bounded(1, 256));
            m_seeds.insert(level->id, seed);
        }
        return bytesOf({0x67, subFunction}) + seed;
    }

    if (!m_seeds.contains(level->id))
        return negative(0x27, 0x24); // a key without asking for a seed first
    if (request.size() != 2 + level->keyLength)
        return negative(0x27, 0x13);
    const QByteArray seed = m_seeds.take(level->id);
    if (request.mid(2) != computeKey(seed, level->keyLength)) {
        const int failed = m_failedAttempts.value(level->id) + 1;
        if (failed >= level->maxAttempts) {
            m_failedAttempts.remove(level->id);
            m_lockedOutUntil.insert(level->id, m_clock.elapsed() + LockoutTime);
            return negative(0x27, 0x36);
        }
        m_failedAttempts.insert(level->id, failed);
        return negative(0x27, 0x35);
    }
    m_failedAttempts.remove(level->id);
    m_unlockedLevelIds.insert(level->id);
    return bytesOf({0x67, subFunction});
}

QByteArray EcuSimulator::writeDataByIdentifier(const QByteArray &request)
{
    if (request.size() < 3)
        return negative(0x2E, 0x13);
    const quint16 didNumber = quint16((quint8(request[1]) << 8) | quint8(request[2]));
    const usc::DataIdentifier *did = findDid(*m_description, didNumber);
    if (!did || !usc::isWritable(*did))
        return negative(0x2E, 0x31);
    if (request.size() != 3 + did->length)
        return negative(0x2E, 0x13);
    if (!inSession(did->writeSessionIds))
        return negative(0x2E, 0x31);
    if (!isUnlocked(did->writeSecurityLevelIds))
        return negative(0x2E, 0x33);
    m_dataRecords.insert(didNumber, request.mid(3));
    return bytesOf({0x6E, quint8(request[1]), quint8(request[2])});
}

QByteArray EcuSimulator::testerPresent(const QByteArray &request)
{
    if (request.size() != 2)
        return negative(0x3E, 0x13);
    if ((quint8(request[1]) & 0x7F) != 0x00)
        return negative(0x3E, 0x12);
    return bytesOf({0x7E, 0x00});
}

bool EcuSimulator::inSession(const QVector<int> &sessionIds) const
{
    return sessionIds.contains(m_sessionId);
}

bool EcuSimulator::isUnlocked(const QVector<int> &securityLevelIds) const
{
    if (securityLevelIds.isEmpty())
        return true;
    for (int id : securityLevelIds) {
        if (m_unlockedLevelIds.contains(id))
            return true;
    }
    return false;
}

void EcuSimulator::enterDefaultSession()
{
    if (!m_description)
        return;
    const usc::Session *session = defaultSession(*m_description);
    m_sessionId = session ? session->id : 0;
    m_unlockedLevelIds.clear();
    m_seeds.clear();
    m_s3Timer->stop();
}

void EcuSimulator::sendResponse(const QByteArray &response)
{
    if (m_description && m_bus->isOpen())
        m_bus->send(m_responseId, response);
}

void EcuSimulator::sendBusTraffic()
{
    if (!m_database)
        return;
    const qint64 now = m_clock.elapsed();
    if (now < m_silentUntil)
        return;
    const double seconds = now / 1000.0;
    for (const ff::Message &message : m_database->allMessages()) {
        if (!ff::isCyclic(message.txMethod) || message.cycleTime <= 0)
            continue;
        const ff::Node *transmitter = m_database->findNode(message.transmitterId);
        if (transmitter && transmitter->name.compare("Tester", Qt::CaseInsensitive) == 0)
            continue; // the tester's own messages are sent with the Interactive generator
        if (now < m_nextTransmit.value(message.id, 0))
            continue;
        m_nextTransmit.insert(message.id, now + message.cycleTime);

        QByteArray payload(message.length, '\0');
        int index = 0;
        for (const ff::SignalMapping &mapping : message.signalMappings) {
            const ff::Signal *signal = m_database->findSignal(mapping.signalId);
            if (!signal)
                continue;
            ++index;
            const ff::ValueTable *table = m_database->findValueTable(signal->valueTableId);
            if (table && !table->entries.isEmpty()) {
                // Step through the named values, one every few seconds.
                const int step = int(seconds / (2 + index)) % table->entries.size();
                const double raw = double(table->entries[step].value);
                encodeSignal(*signal, mapping.startBit, raw * signal->factor + signal->offset, &payload);
                continue;
            }
            // A slow wave between minimum and maximum, each signal at its own pace.
            const double period = 6.0 + 3.0 * index + (message.frameId % 7);
            const double wave = 0.5 - 0.5 * std::cos(2 * M_PI * seconds / period);
            const double low = signal->minimum;
            const double high = signal->maximum > signal->minimum ? signal->maximum : signal->minimum + 1;
            encodeSignal(*signal, mapping.startBit, low + (high - low) * wave, &payload);
        }
        m_bus->send(message.frameId, payload);
    }
}
