#include "tester.h"
#include "decoding.h"
#include "diagdefaults.h"
#include "keyalgorithm.h"
#include "udpbus.h"

#include <QTimer>

Tester::Tester(QObject *parent)
    : QObject(parent)
    , m_bus(new UdpBus(this))
    , m_responseTimer(new QTimer(this))
    , m_testerPresentTimer(new QTimer(this))
    , m_s3Timer(new QTimer(this))
{
    connect(m_bus, &UdpBus::received, this, &Tester::receive);
    m_responseTimer->setSingleShot(true);
    connect(m_responseTimer, &QTimer::timeout, this, &Tester::responseTimeout);
    m_testerPresentTimer->setInterval(TesterPresentInterval);
    connect(m_testerPresentTimer, &QTimer::timeout, this, &Tester::sendTesterPresent);
    m_s3Timer->setInterval(100);
    connect(m_s3Timer, &QTimer::timeout, this, &Tester::checkS3);
}

void Tester::setDatabases(const usc::Database *description, const ff::Database *database)
{
    m_description = description;
    m_database = database;
}

bool Tester::start(QString *errorMessage)
{
    stop();

    // The UDP settings: the diagnostic description's, else the database's.
    QString localAddress, remoteAddress;
    int localPort = 0, remotePort = 0, idSize = 4;
    bool unicast = true;
    if (m_description && m_description->network().protocol == usc::Network::Protocol::Udp) {
        const usc::UdpSettings &udp = m_description->network().udp;
        localAddress = udp.localAddress;
        localPort = udp.localPort;
        remoteAddress = udp.remoteAddress;
        remotePort = udp.remotePort;
        idSize = udp.idSize;
        unicast = udp.mode == usc::UdpSettings::Mode::Unicast;
        m_requestId = m_description->network().requestId;
        m_responseId = m_description->network().responseId;
    } else if (!m_description && m_database && m_database->network().protocol == ff::Network::Protocol::Udp) {
        const ff::UdpSettings &udp = m_database->network().udp;
        localAddress = udp.localAddress;
        localPort = udp.localPort;
        remoteAddress = udp.remoteAddress;
        remotePort = udp.remotePort;
        idSize = udp.idSize;
        unicast = udp.mode == ff::UdpSettings::Mode::Unicast;
    } else if (!m_description && !m_database) {
        *errorMessage = "Open a diagnostic description or a database first.";
        return false;
    } else {
        *errorMessage = "DiagProbe only talks UDP for now: set the network's protocol to UDP.";
        return false;
    }
    if (!unicast) {
        *errorMessage = "DiagProbe only talks UDP unicast for now.";
        return false;
    }

    const QHostAddress bindAddress(localAddress);
    if (!m_bus->open(bindAddress, quint16(localPort), QHostAddress(remoteAddress), quint16(remotePort), idSize,
                     errorMessage))
        return false;

    m_clock.start();
    m_running = true;
    m_receivedCount = 0;
    m_sentCount = 0;
    m_pendingRequest.clear();
    m_lastResponse.clear();
    m_lastRequestTime = 0.0;
    enterDefaultSession();
    m_testerPresentTimer->start();
    m_s3Timer->start();
    emit runningChanged(true);
    emit stateChanged();
    return true;
}

void Tester::stop()
{
    if (!m_running)
        return;
    m_responseTimer->stop();
    m_testerPresentTimer->stop();
    m_s3Timer->stop();
    m_bus->close();
    m_pendingRequest.clear();
    m_unlockLevelId = 0;
    m_running = false;
    emit runningChanged(false);
    emit stateChanged();
}

double Tester::time() const
{
    return m_running ? m_clock.elapsed() / 1000.0 : 0.0;
}

bool Tester::sendRequest(const QByteArray &request)
{
    if (!m_running || isBusy() || request.isEmpty())
        return false;
    m_lastRequestTime = time();
    transmit(m_requestId, request);
    emit requestSent(time(), request);

    // TesterPresent with suppressPosRspMsgIndicationBit gets no response to wait for.
    const bool suppressed = request.size() == 2 && quint8(request[0]) == 0x3E && (quint8(request[1]) & 0x80);
    if (!suppressed) {
        m_pendingRequest = request;
        m_responseTimer->start(currentP2() + ResponseTimeMargin);
    }
    emit stateChanged();
    return true;
}

bool Tester::unlock(int securityLevelId, QString *errorMessage)
{
    QString error;
    const usc::SecurityLevel *level = m_description ? m_description->findSecurityLevel(securityLevelId) : nullptr;
    if (!m_keyAlgorithm || !m_keyAlgorithm->isLoaded())
        error = "No key algorithm selected (File → Select Key Algorithm).";
    else if (!level)
        error = "Unknown security level.";
    else if (!sendRequest(bytesOf({0x27, level->requestSeed})))
        error = isRunning() ? "Another request is still waiting for its response." : "The measurement is not running.";
    if (!error.isEmpty()) {
        if (errorMessage)
            *errorMessage = error;
        return false;
    }
    m_unlockLevelId = securityLevelId;
    return true;
}

void Tester::setTesterPresentEnabled(bool enabled)
{
    m_testerPresentEnabled = enabled;
    emit stateChanged();
}

bool Tester::isUnlocked(const QVector<int> &securityLevelIds) const
{
    if (securityLevelIds.isEmpty())
        return true;
    for (int id : securityLevelIds) {
        if (m_unlockedLevelIds.contains(id))
            return true;
    }
    return false;
}

double Tester::s3Remaining() const
{
    const usc::Session *session = m_description ? m_description->findSession(m_sessionId) : nullptr;
    if (!m_running || !session || session->sessionType == usc::DefaultSessionType)
        return -1;
    return qMax(0.0, m_lastRequestTime + S3Time / 1000.0 - time());
}

void Tester::sendFrame(quint32 id, const QByteArray &payload)
{
    if (m_running)
        transmit(id, payload);
}

void Tester::transmit(quint32 id, const QByteArray &payload)
{
    m_bus->send(id, payload);
    ++m_sentCount;
    emit frameSeen(time(), id, payload, true);
}

void Tester::receive(quint32 id, const QByteArray &payload)
{
    ++m_receivedCount;
    emit frameSeen(time(), id, payload, false);
    if (m_description && id == m_responseId && !payload.isEmpty())
        handleResponse(payload);
}

void Tester::handleResponse(const QByteArray &response)
{
    if (m_pendingRequest.isEmpty())
        return; // nobody asked: a late or unexpected response
    const quint8 serviceId = quint8(m_pendingRequest[0]);
    const bool negative = isNegativeResponse(response);
    if (negative ? (response.size() < 3 || quint8(response[1]) != serviceId)
                 : quint8(response[0]) != serviceId + 0x40)
        return; // the response to some other request

    const QByteArray request = m_pendingRequest;
    if (negative && quint8(response[2]) == 0x78) {
        // Response pending: the ECU needs longer, so wait P2* from now.
        m_responseTimer->start(currentP2Star() + ResponseTimeMargin);
        m_lastResponse = describeUds(m_description, response);
        emit responseReceived(time(), request, response);
        emit stateChanged();
        return;
    }
    m_responseTimer->stop();
    m_pendingRequest.clear();

    // What a positive response tells about the ECU's state.
    if (!negative) {
        const quint8 subFunction = response.size() > 1 ? quint8(response[1]) : 0;
        if (serviceId == 0x10) {
            const usc::Session *session = findSessionByType(*m_description, subFunction);
            m_sessionId = session ? session->id : m_sessionId;
            m_unlockedLevelIds.clear();
        } else if (serviceId == 0x11) {
            enterDefaultSession();
        } else if (serviceId == 0x27 && subFunction % 2 == 0) {
            if (const usc::SecurityLevel *level = findLevelByRequestSeed(*m_description, subFunction - 1))
                m_unlockedLevelIds.insert(level->id);
        } else if (serviceId == 0x27) {
            const QByteArray seed = response.mid(2);
            const usc::SecurityLevel *level = findLevelByRequestSeed(*m_description, subFunction);
            if (level && !seed.isEmpty() && seed.count('\0') == seed.size())
                m_unlockedLevelIds.insert(level->id); // all zero: already unlocked
        }
    }
    m_lastResponse = describeUds(m_description, response);
    emit responseReceived(time(), request, response);

    // The second half of unlock(): the key for the seed.
    if (m_unlockLevelId != 0 && serviceId == 0x27) {
        const usc::SecurityLevel *level = m_description->findSecurityLevel(m_unlockLevelId);
        const quint8 subFunction = response.size() > 1 ? quint8(response[1]) : 0;
        m_unlockLevelId = 0;
        if (!negative && level && subFunction == level->requestSeed && !m_unlockedLevelIds.contains(level->id)) {
            QByteArray key;
            QString error;
            if (m_keyAlgorithm && m_keyAlgorithm->computeKey(level->requestSeed, response.mid(2), level->keyLength,
                                                             &key, &error)) {
                const QByteArray sendKey = bytesOf({0x27, usc::sendKey(level->requestSeed)}) + key;
                QTimer::singleShot(0, this, [this, sendKey] { sendRequest(sendKey); });
            } else {
                m_lastResponse = error;
                emit keyFailed(time(), error);
            }
        }
    }
    emit stateChanged();
}

void Tester::responseTimeout()
{
    const QByteArray request = m_pendingRequest;
    m_pendingRequest.clear();
    m_unlockLevelId = 0;
    m_lastResponse = "No response (timeout)";
    emit requestTimedOut(time(), request);
    emit stateChanged();
}

void Tester::sendTesterPresent()
{
    if (m_testerPresentEnabled && s3Remaining() >= 0 && !isBusy())
        sendRequest(bytesOf({0x3E, 0x80}));
}

void Tester::checkS3()
{
    if (s3Remaining() == 0.0 && !isBusy()) {
        // The ECU has fallen back to the default session by itself.
        enterDefaultSession();
        m_lastResponse = "S3 expired: the ECU is back in the default session";
        emit stateChanged();
    }
}

void Tester::enterDefaultSession()
{
    const usc::Session *session = m_description ? defaultSession(*m_description) : nullptr;
    m_sessionId = session ? session->id : 0;
    m_unlockedLevelIds.clear();
}

int Tester::currentP2() const
{
    const usc::Session *session = m_description ? m_description->findSession(m_sessionId) : nullptr;
    return session ? session->p2 : 50;
}

int Tester::currentP2Star() const
{
    const usc::Session *session = m_description ? m_description->findSession(m_sessionId) : nullptr;
    return session ? session->p2Star : 5000;
}
