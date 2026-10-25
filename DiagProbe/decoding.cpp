#include "decoding.h"
#include "bitcodec.h"

#include <QRegularExpression>

namespace {

BitCodec::Kind kindOf(ff::Signal::ValueType valueType)
{
    switch (valueType) {
    case ff::Signal::ValueType::Signed:     return BitCodec::Kind::Signed;
    case ff::Signal::ValueType::Unsigned:   return BitCodec::Kind::Unsigned;
    case ff::Signal::ValueType::IeeeFloat:  return BitCodec::Kind::Float;
    case ff::Signal::ValueType::IeeeDouble: return BitCodec::Kind::Double;
    }
    return BitCodec::Kind::Unsigned;
}

BitCodec::Kind kindOf(usc::DataType::ValueType valueType)
{
    switch (valueType) {
    case usc::DataType::ValueType::Signed:     return BitCodec::Kind::Signed;
    case usc::DataType::ValueType::IeeeFloat:  return BitCodec::Kind::Float;
    case usc::DataType::ValueType::IeeeDouble: return BitCodec::Kind::Double;
    default:                                   return BitCodec::Kind::Unsigned;
    }
}

template<typename Table>
QString lookUp(const Table *table, qint64 value)
{
    if (!table)
        return QString();
    for (const auto &entry : table->entries) {
        if (entry.value == value)
            return entry.description;
    }
    return QString();
}

// ASCII and Bytes are byte strings: they start at byte startBit / 8, whatever the byte order.
int firstByte(const usc::DataTypeMapping &mapping)
{
    return mapping.startBit / 8;
}

} // namespace

QString hexBytes(const QByteArray &bytes)
{
    return QString::fromLatin1(bytes.toHex(' ').toUpper());
}

bool parseHex(const QString &text, QByteArray *bytes)
{
    QString compact = text;
    compact.remove(QRegularExpression("\\s"));
    if (compact.startsWith("0x", Qt::CaseInsensitive))
        compact = compact.mid(2);
    if (compact.size() % 2 != 0 || !QRegularExpression("^[0-9A-Fa-f]*$").match(compact).hasMatch())
        return false;
    *bytes = QByteArray::fromHex(compact.toLatin1());
    return true;
}

QString formatNumber(double value)
{
    return QString::number(value, 'g', 10);
}

QVector<DecodedValue> decodeMessage(const ff::Database &database, const ff::Message &message,
                                    const QByteArray &payload)
{
    QVector<DecodedValue> values;
    for (const ff::SignalMapping &mapping : message.signalMappings) {
        const ff::Signal *signal = database.findSignal(mapping.signalId);
        if (!signal)
            continue;
        const QVector<int> bits = ff::signalBits(mapping.startBit, signal->length, signal->byteOrder);
        const bool lsbFirst = signal->byteOrder == ff::Signal::ByteOrder::Intel;
        const quint64 raw = BitCodec::extract(payload, bits, lsbFirst);
        const BitCodec::Kind kind = kindOf(signal->valueType);
        DecodedValue value;
        value.name = signal->name;
        value.raw = QString::number(BitCodec::rawInteger(raw, signal->length, kind));
        value.physical = BitCodec::toPhysical(raw, signal->length, kind, signal->factor, signal->offset);
        value.physicalText = formatNumber(value.physical);
        value.unit = signal->unit;
        value.valueText = lookUp(database.findValueTable(signal->valueTableId),
                                 BitCodec::rawInteger(raw, signal->length, kind));
        values.append(value);
    }
    return values;
}

double decodeSignal(const ff::Signal &signal, int startBit, const QByteArray &payload)
{
    const QVector<int> bits = ff::signalBits(startBit, signal.length, signal.byteOrder);
    const quint64 raw = BitCodec::extract(payload, bits, signal.byteOrder == ff::Signal::ByteOrder::Intel);
    return BitCodec::toPhysical(raw, signal.length, kindOf(signal.valueType), signal.factor, signal.offset);
}

void encodeSignal(const ff::Signal &signal, int startBit, double physical, QByteArray *payload)
{
    const QVector<int> bits = ff::signalBits(startBit, signal.length, signal.byteOrder);
    const quint64 raw = BitCodec::fromPhysical(physical, signal.length, kindOf(signal.valueType),
                                               signal.factor, signal.offset);
    BitCodec::insert(*payload, bits, signal.byteOrder == ff::Signal::ByteOrder::Intel, raw);
}

const ff::Message *findMessageByFrameId(const ff::Database &database, quint32 frameId)
{
    for (const ff::Message &message : database.allMessages()) {
        if (message.frameId == frameId)
            return &message;
    }
    return nullptr;
}

const usc::Session *findSessionByType(const usc::Database &description, quint8 sessionType)
{
    for (const usc::Session &session : description.allSessions()) {
        if (session.sessionType == sessionType)
            return &session;
    }
    return nullptr;
}

const usc::Session *defaultSession(const usc::Database &description)
{
    return findSessionByType(description, usc::DefaultSessionType);
}

const usc::ResetType *findResetTypeByValue(const usc::Database &description, quint8 resetType)
{
    for (const usc::ResetType &candidate : description.allResetTypes()) {
        if (candidate.resetType == resetType)
            return &candidate;
    }
    return nullptr;
}

const usc::SecurityLevel *findLevelByRequestSeed(const usc::Database &description, quint8 requestSeed)
{
    for (const usc::SecurityLevel &level : description.allSecurityLevels()) {
        if (level.requestSeed == requestSeed)
            return &level;
    }
    return nullptr;
}

const usc::DataIdentifier *findDid(const usc::Database &description, quint16 did)
{
    for (const usc::DataIdentifier &dataIdentifier : description.allDataIdentifiers()) {
        if (dataIdentifier.did == did)
            return &dataIdentifier;
    }
    return nullptr;
}

QVector<DecodedValue> decodeDataRecord(const usc::Database &description, const usc::DataIdentifier &did,
                                       const QByteArray &record)
{
    QVector<DecodedValue> values;
    for (const usc::DataTypeMapping &mapping : did.mappings) {
        const usc::DataType *dataType = description.findDataType(mapping.dataTypeId);
        if (!dataType)
            continue;
        DecodedValue value;
        value.name = mapping.name;
        if (usc::needsWholeBytes(dataType->valueType)) {
            const QByteArray bytes = record.mid(firstByte(mapping), dataType->length / 8);
            value.isNumber = false;
            value.raw = hexBytes(bytes);
            if (dataType->valueType == usc::DataType::ValueType::Ascii) {
                QString text;
                for (char c : bytes)
                    text += (c >= 0x20 && c < 0x7F) ? QChar(c) : QChar('.');
                value.physicalText = text;
            } else {
                value.physicalText = value.raw;
            }
        } else {
            const QVector<int> bits = usc::dataTypeBits(mapping.startBit, dataType->length, dataType->byteOrder);
            const bool lsbFirst = dataType->byteOrder == usc::DataType::ByteOrder::Intel;
            const quint64 raw = BitCodec::extract(record, bits, lsbFirst);
            const BitCodec::Kind kind = kindOf(dataType->valueType);
            const qint64 integer = BitCodec::rawInteger(raw, dataType->length, kind);
            value.raw = QString::number(integer);
            value.physical = BitCodec::toPhysical(raw, dataType->length, kind, dataType->factor, dataType->offset);
            value.physicalText = formatNumber(value.physical);
            value.unit = dataType->unit;
            value.valueText = lookUp(description.findValueTable(dataType->valueTableId), integer);
        }
        values.append(value);
    }
    return values;
}

bool encodePlacement(const usc::Database &description, const usc::DataTypeMapping &mapping,
                     const QString &text, QByteArray *record, QString *errorMessage)
{
    const usc::DataType *dataType = description.findDataType(mapping.dataTypeId);
    if (!dataType)
        return true;
    const int byteCount = dataType->length / 8;
    switch (dataType->valueType) {
    case usc::DataType::ValueType::Ascii: {
        const QByteArray latin = text.toLatin1();
        if (latin.size() > byteCount) {
            *errorMessage = QString("'%1' is at most %2 characters long.").arg(mapping.name).arg(byteCount);
            return false;
        }
        record->replace(firstByte(mapping), byteCount, latin.leftJustified(byteCount, ' '));
        return true;
    }
    case usc::DataType::ValueType::Bytes: {
        QByteArray bytes;
        if (!parseHex(text, &bytes) || bytes.size() != byteCount) {
            *errorMessage = QString("'%1' needs exactly %2 bytes of hex.").arg(mapping.name).arg(byteCount);
            return false;
        }
        record->replace(firstByte(mapping), byteCount, bytes);
        return true;
    }
    default:
        break;
    }

    // A number: a value table description stands for its raw value.
    const QVector<int> bits = usc::dataTypeBits(mapping.startBit, dataType->length, dataType->byteOrder);
    const bool lsbFirst = dataType->byteOrder == usc::DataType::ByteOrder::Intel;
    const BitCodec::Kind kind = kindOf(dataType->valueType);
    if (const usc::ValueTable *table = description.findValueTable(dataType->valueTableId)) {
        for (const usc::ValueTable::Entry &entry : table->entries) {
            if (entry.description.compare(text.trimmed(), Qt::CaseInsensitive) == 0) {
                BitCodec::insert(*record, bits, lsbFirst, quint64(entry.value) & BitCodec::mask(dataType->length));
                return true;
            }
        }
    }
    bool ok = false;
    const double physical = text.trimmed().toDouble(&ok);
    if (!ok) {
        *errorMessage = QString("'%1' needs a number.").arg(mapping.name);
        return false;
    }
    BitCodec::insert(*record, bits, lsbFirst,
                     BitCodec::fromPhysical(physical, dataType->length, kind, dataType->factor, dataType->offset));
    return true;
}

QString serviceName(quint8 serviceId)
{
    switch (serviceId) {
    case 0x10: return "DiagnosticSessionControl";
    case 0x11: return "ECUReset";
    case 0x22: return "ReadDataByIdentifier";
    case 0x27: return "SecurityAccess";
    case 0x2E: return "WriteDataByIdentifier";
    case 0x3E: return "TesterPresent";
    }
    return "Service " + usc::subFunctionToText(serviceId);
}

QString nrcName(quint8 nrc)
{
    switch (nrc) {
    case 0x10: return "generalReject";
    case 0x11: return "serviceNotSupported";
    case 0x12: return "subFunctionNotSupported";
    case 0x13: return "incorrectMessageLengthOrInvalidFormat";
    case 0x14: return "responseTooLong";
    case 0x21: return "busyRepeatRequest";
    case 0x22: return "conditionsNotCorrect";
    case 0x24: return "requestSequenceError";
    case 0x31: return "requestOutOfRange";
    case 0x33: return "securityAccessDenied";
    case 0x35: return "invalidKey";
    case 0x36: return "exceededNumberOfAttempts";
    case 0x37: return "requiredTimeDelayNotExpired";
    case 0x72: return "generalProgrammingFailure";
    case 0x78: return "requestCorrectlyReceivedResponsePending";
    case 0x7E: return "subFunctionNotSupportedInActiveSession";
    case 0x7F: return "serviceNotSupportedInActiveSession";
    }
    return "unknown";
}

QString nrcText(quint8 nrc)
{
    return usc::subFunctionToText(nrc) + ' ' + nrcName(nrc);
}

bool isNegativeResponse(const QByteArray &response)
{
    return !response.isEmpty() && quint8(response[0]) == 0x7F;
}

namespace {

quint16 didOf(const QByteArray &message)
{
    return quint16((quint8(message[1]) << 8) | quint8(message[2]));
}

QString didLabel(const usc::Database *description, quint16 did)
{
    const usc::DataIdentifier *dataIdentifier = description ? findDid(*description, did) : nullptr;
    return usc::didToText(did) + (dataIdentifier ? ' ' + dataIdentifier->name : QString());
}

QString sessionLabel(const usc::Database *description, quint8 sessionType)
{
    const usc::Session *session = description ? findSessionByType(*description, sessionType) : nullptr;
    return session ? session->name : usc::subFunctionToText(sessionType);
}

QString resetLabel(const usc::Database *description, quint8 resetType)
{
    const usc::ResetType *reset = description ? findResetTypeByValue(*description, resetType) : nullptr;
    return reset ? reset->name : usc::subFunctionToText(resetType);
}

QString levelLabel(const usc::Database *description, quint8 requestSeed)
{
    const usc::SecurityLevel *level = description ? findLevelByRequestSeed(*description, requestSeed) : nullptr;
    return level ? level->name : "level " + usc::subFunctionToText(requestSeed);
}

QString describeRequest(const usc::Database *description, const QByteArray &request)
{
    const quint8 serviceId = quint8(request[0]);
    const quint8 subFunction = request.size() > 1 ? quint8(request[1]) & 0x7F : 0;
    const bool suppress = request.size() > 1 && (quint8(request[1]) & 0x80);
    const QString noResponse = suppress ? " (no positive response wanted)" : "";
    switch (serviceId) {
    case 0x10:
        if (request.size() == 2)
            return "Switch to " + sessionLabel(description, subFunction) + noResponse;
        break;
    case 0x11:
        if (request.size() == 2)
            return resetLabel(description, subFunction) + noResponse;
        break;
    case 0x22:
        if (request.size() == 3)
            return "Read " + didLabel(description, didOf(request));
        break;
    case 0x27:
        if (request.size() >= 2) {
            if (subFunction % 2 == 1)
                return "Request seed for " + levelLabel(description, subFunction);
            return "Send key for " + levelLabel(description, subFunction - 1) + ": " + hexBytes(request.mid(2));
        }
        break;
    case 0x2E:
        if (request.size() >= 3)
            return "Write " + didLabel(description, didOf(request)) + ": " + hexBytes(request.mid(3));
        break;
    case 0x3E:
        return "Keep the session alive" + noResponse;
    }
    return serviceName(serviceId);
}

QString describePositive(const usc::Database *description, const QByteArray &response)
{
    const quint8 serviceId = quint8(response[0]) - 0x40;
    const quint8 subFunction = response.size() > 1 ? quint8(response[1]) : 0;
    switch (serviceId) {
    case 0x10:
        if (response.size() == 6) {
            const int p2 = (quint8(response[2]) << 8) | quint8(response[3]);
            const int p2Star = ((quint8(response[4]) << 8) | quint8(response[5])) * 10;
            return QString("Positive: %1 session, P2 %2 ms, P2* %3 ms")
                .arg(sessionLabel(description, subFunction)).arg(p2).arg(p2Star);
        }
        break;
    case 0x11:
        if (response.size() == 3)
            return QString("Positive: %1, power down in %2 s").arg(resetLabel(description, subFunction))
                .arg(quint8(response[2]));
        return "Positive: " + resetLabel(description, subFunction) + ", ECU resets";
    case 0x22:
        if (response.size() >= 3)
            return QString("Positive: %1, %2 bytes").arg(didLabel(description, didOf(response)))
                .arg(response.size() - 3);
        break;
    case 0x27:
        if (subFunction % 2 == 1) {
            const QByteArray seed = response.mid(2);
            if (!seed.isEmpty() && seed.count('\0') == seed.size())
                return "Positive: " + levelLabel(description, subFunction) + " is already unlocked";
            return "Positive: seed for " + levelLabel(description, subFunction) + ": " + hexBytes(seed);
        }
        return "Positive: " + levelLabel(description, subFunction - 1) + " unlocked";
    case 0x2E:
        if (response.size() >= 3)
            return "Positive: " + didLabel(description, didOf(response)) + " written";
        break;
    case 0x3E:
        return "Positive: session kept alive";
    }
    return "Positive response to " + serviceName(serviceId);
}

} // namespace

QString describeUds(const usc::Database *description, const QByteArray &message)
{
    if (message.isEmpty())
        return QString();
    const quint8 serviceId = quint8(message[0]);
    if (serviceId == 0x7F) {
        if (message.size() < 3)
            return "Negative response (too short)";
        return QString("Negative: %1 (%2)").arg(nrcText(quint8(message[2])), serviceName(quint8(message[1])));
    }
    if (serviceId >= 0x40)
        return describePositive(description, message);
    return describeRequest(description, message);
}
