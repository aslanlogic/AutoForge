#include "xmlstorage.h"
#include "database.h"

#include <QFile>
#include <QHash>
#include <QLocale>
#include <QSaveFile>
#include <QSet>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>

namespace usc {

namespace {

const QString FormatVersion = "1";

// Reads one <ValueTable> element. On a problem it calls reader.raiseError().
ValueTable readValueTable(QXmlStreamReader &reader)
{
    ValueTable valueTable;
    valueTable.name = reader.attributes().value("name").toString().trimmed();
    if (valueTable.name.isEmpty())
        reader.raiseError("A value table has no name.");

    QSet<qint64> values;
    while (!reader.hasError() && reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Value")) {
            // value is written in decimal; hex with 0x in front is accepted too.
            const QString text = reader.attributes().value("value").toString().trimmed();
            ValueTable::Entry entry;
            bool ok = false;
            if (text.startsWith("0x", Qt::CaseInsensitive))
                entry.value = text.mid(2).toLongLong(&ok, 16);
            else
                entry.value = text.toLongLong(&ok, 10);
            entry.description = reader.attributes().value("description").toString();
            if (!ok)
                reader.raiseError(QString("Value table '%1': '%2' is not a whole number.").arg(valueTable.name, text));
            else if (values.contains(entry.value))
                reader.raiseError(QString("Value table '%1': the value %2 is there twice.")
                                      .arg(valueTable.name).arg(entry.value));
            values.insert(entry.value);
            valueTable.entries.append(entry);
            reader.skipCurrentElement();
        } else if (reader.name() == QLatin1String("Comment")) {
            valueTable.comment = reader.readElementText();
        } else {
            reader.skipCurrentElement();
        }
    }

    std::sort(valueTable.entries.begin(), valueTable.entries.end(),
              [](const ValueTable::Entry &a, const ValueTable::Entry &b) { return a.value < b.value; });
    return valueTable;
}

// Words used in the file. They are kept separate from the texts shown in the
// GUI, so renaming a GUI label never breaks existing files.
QString byteOrderToXml(DataType::ByteOrder byteOrder)
{
    switch (byteOrder) {
    case DataType::ByteOrder::Intel:    return "intel";
    case DataType::ByteOrder::Motorola: return "motorola";
    }
    return QString();
}

QString valueTypeToXml(DataType::ValueType valueType)
{
    switch (valueType) {
    case DataType::ValueType::Signed:     return "signed";
    case DataType::ValueType::Unsigned:   return "unsigned";
    case DataType::ValueType::IeeeFloat:  return "float";
    case DataType::ValueType::IeeeDouble: return "double";
    case DataType::ValueType::Ascii:      return "ascii";
    case DataType::ValueType::Bytes:      return "bytes";
    }
    return QString();
}

// Reads one <DataType> element. On a problem it calls reader.raiseError().
// Its value table is referred to by name; the caller turns that into an id.
DataType readDataType(QXmlStreamReader &reader)
{
    const QXmlStreamAttributes attributes = reader.attributes();
    const QLocale c = QLocale::c();
    DataType dataType;

    auto number = [&](const QString &name, double defaultValue) {
        if (!attributes.hasAttribute(name))
            return defaultValue;
        bool ok = false;
        const double value = c.toDouble(attributes.value(name).toString(), &ok);
        if (!ok)
            reader.raiseError(QString("Data type '%1': '%2' is not a number.").arg(dataType.name, name));
        return value;
    };

    dataType.name = attributes.value("name").toString().trimmed();
    if (dataType.name.isEmpty())
        reader.raiseError("A data type has no name.");

    const QString byteOrder = attributes.value("byteOrder").toString();
    if (byteOrder == "intel" || byteOrder.isEmpty())
        dataType.byteOrder = DataType::ByteOrder::Intel;
    else if (byteOrder == "motorola")
        dataType.byteOrder = DataType::ByteOrder::Motorola;
    else
        reader.raiseError(QString("Data type '%1': unknown byteOrder '%2'.").arg(dataType.name, byteOrder));

    const QString valueType = attributes.value("valueType").toString();
    if (valueType == "unsigned" || valueType.isEmpty())
        dataType.valueType = DataType::ValueType::Unsigned;
    else if (valueType == "signed")
        dataType.valueType = DataType::ValueType::Signed;
    else if (valueType == "float")
        dataType.valueType = DataType::ValueType::IeeeFloat;
    else if (valueType == "double")
        dataType.valueType = DataType::ValueType::IeeeDouble;
    else if (valueType == "ascii")
        dataType.valueType = DataType::ValueType::Ascii;
    else if (valueType == "bytes")
        dataType.valueType = DataType::ValueType::Bytes;
    else
        reader.raiseError(QString("Data type '%1': unknown valueType '%2'.").arg(dataType.name, valueType));

    const double length = number("length", dataType.length);
    dataType.length = int(length);
    const QString lengthError = checkLength(dataType);
    if (!reader.hasError() && (dataType.length != length || !lengthError.isEmpty()))
        reader.raiseError(QString("Data type '%1': %2").arg(dataType.name,
                                                             lengthError.isEmpty() ? "the length must be a whole number."
                                                                                   : lengthError));

    dataType.unit = attributes.value("unit").toString();
    dataType.factor = number("factor", dataType.factor);
    dataType.offset = number("offset", dataType.offset);
    dataType.minimum = number("minimum", dataType.minimum);
    dataType.maximum = number("maximum", dataType.maximum);

    // Child elements; ones we do not know (e.g. from a newer version) are skipped.
    while (!reader.hasError() && reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Comment"))
            dataType.comment = reader.readElementText();
        else
            reader.skipCurrentElement();
    }

    return dataType;
}

// What a <DataIdentifier> element refers to by name; turned into ids once the
// whole file is read.
struct DataIdentifierRefs
{
    QStringList dataTypes; // per placement, in the order of DataIdentifier::mappings
    QStringList readSessions;
    QStringList readSecurityLevels;
};

// Reads one <DataIdentifier> element. On a problem it calls reader.raiseError().
DataIdentifier readDataIdentifier(QXmlStreamReader &reader, DataIdentifierRefs *refs)
{
    const QXmlStreamAttributes attributes = reader.attributes();
    DataIdentifier dataIdentifier;

    dataIdentifier.name = attributes.value("name").toString().trimmed();
    if (dataIdentifier.name.isEmpty())
        reader.raiseError("A DID has no name.");

    // did is written as hex ("0xF190"); plain decimal is accepted too.
    const QString did = attributes.value("did").toString().trimmed();
    bool ok = false;
    uint value = 0;
    if (did.startsWith("0x", Qt::CaseInsensitive))
        value = did.mid(2).toUInt(&ok, 16);
    else
        value = did.toUInt(&ok, 10);
    if (!ok || value > 0xFFFF)
        reader.raiseError(QString("DID '%1': did '%2' is not a number from 0x0000 to 0xFFFF.").arg(dataIdentifier.name, did));
    dataIdentifier.did = quint16(value);

    if (attributes.hasAttribute("length")) {
        dataIdentifier.length = attributes.value("length").toInt(&ok);
        if (!ok || dataIdentifier.length < 1 || dataIdentifier.length > MaxDataRecordLength)
            reader.raiseError(QString("DID '%1': 'length' must be a whole number from 1 to %2.")
                                  .arg(dataIdentifier.name).arg(MaxDataRecordLength));
    }

    QSet<QString> placementNames;
    while (!reader.hasError() && reader.readNextStartElement()) {
        const QXmlStreamAttributes child = reader.attributes();
        if (reader.name() == QLatin1String("Comment")) {
            dataIdentifier.comment = reader.readElementText();
            continue;
        }
        if (reader.name() == QLatin1String("DataType")) {
            DataTypeMapping mapping;
            mapping.name = child.value("name").toString().trimmed();
            mapping.startBit = child.value("startBit").toInt(&ok);
            if (mapping.name.isEmpty())
                reader.raiseError(QString("DID '%1': a placed data type has no name.").arg(dataIdentifier.name));
            else if (placementNames.contains(mapping.name))
                reader.raiseError(QString("DID '%1': there are two placed data types named '%2'.")
                                      .arg(dataIdentifier.name, mapping.name));
            else if (!ok || mapping.startBit < 0)
                reader.raiseError(QString("DID '%1': '%2' has no valid startBit.").arg(dataIdentifier.name, mapping.name));
            placementNames.insert(mapping.name);
            dataIdentifier.mappings.append(mapping);
            refs->dataTypes.append(child.value("dataType").toString());
        } else if (reader.name() == QLatin1String("ReadSession")) {
            refs->readSessions.append(child.value("name").toString());
        } else if (reader.name() == QLatin1String("ReadSecurityLevel")) {
            refs->readSecurityLevels.append(child.value("name").toString());
        }
        reader.skipCurrentElement();
    }
    return dataIdentifier;
}

// Reads one <Session> element. On a problem it calls reader.raiseError().
Session readSession(QXmlStreamReader &reader)
{
    const QXmlStreamAttributes attributes = reader.attributes();
    Session session;

    auto integer = [&](const QString &name, int defaultValue, int minimum, int maximum) {
        if (!attributes.hasAttribute(name))
            return defaultValue;
        bool ok = false;
        const int value = attributes.value(name).toInt(&ok);
        if (!ok || value < minimum || value > maximum)
            reader.raiseError(QString("Session '%1': '%2' must be a whole number from %3 to %4.")
                                  .arg(session.name, name).arg(minimum).arg(maximum));
        return value;
    };

    session.name = attributes.value("name").toString().trimmed();
    if (session.name.isEmpty())
        reader.raiseError("A session has no name.");

    // sessionType is written as hex ("0x01"); plain decimal is accepted too.
    const QString sessionType = attributes.value("sessionType").toString().trimmed();
    bool ok = false;
    uint value = 0;
    if (sessionType.startsWith("0x", Qt::CaseInsensitive))
        value = sessionType.mid(2).toUInt(&ok, 16);
    else
        value = sessionType.toUInt(&ok, 10);
    if (!ok || value > 0xFF || !isValidSessionType(quint8(value)))
        reader.raiseError(QString("Session '%1': sessionType '%2' is not a valid sub-function.")
                              .arg(session.name, sessionType));
    session.sessionType = quint8(value);

    // The largest values the positive response can report (see session.h).
    session.p2 = integer("p2", session.p2, 0, 65535);
    session.p2Star = integer("p2Star", session.p2Star, 0, 655350);
    if (!reader.hasError() && session.p2Star % P2StarStep != 0)
        reader.raiseError(QString("Session '%1': 'p2Star' must be a multiple of %2 ms.").arg(session.name).arg(P2StarStep));

    while (!reader.hasError() && reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Comment"))
            session.comment = reader.readElementText();
        else
            reader.skipCurrentElement();
    }
    return session;
}

// Reads one <SecurityLevel> element. On a problem it calls reader.raiseError().
// The names of its allowed sessions go to *sessionNames; they are turned into
// ids once all sessions are read.
SecurityLevel readSecurityLevel(QXmlStreamReader &reader, QStringList *sessionNames)
{
    const QXmlStreamAttributes attributes = reader.attributes();
    SecurityLevel securityLevel;

    auto integer = [&](const QString &name, int defaultValue, int minimum) {
        if (!attributes.hasAttribute(name))
            return defaultValue;
        bool ok = false;
        const int value = attributes.value(name).toInt(&ok);
        if (!ok || value < minimum)
            reader.raiseError(QString("Security level '%1': '%2' must be a whole number of at least %3.")
                                  .arg(securityLevel.name, name).arg(minimum));
        return value;
    };

    securityLevel.name = attributes.value("name").toString().trimmed();
    if (securityLevel.name.isEmpty())
        reader.raiseError("A security level has no name.");

    // requestSeed is written as hex ("0x01"); plain decimal is accepted too.
    const QString requestSeed = attributes.value("requestSeed").toString().trimmed();
    bool ok = false;
    uint value = 0;
    if (requestSeed.startsWith("0x", Qt::CaseInsensitive))
        value = requestSeed.mid(2).toUInt(&ok, 16);
    else
        value = requestSeed.toUInt(&ok, 10);
    if (!ok || value > 0xFF || !isValidRequestSeed(quint8(value)))
        reader.raiseError(QString("Security level '%1': requestSeed '%2' is not a valid sub-function.")
                              .arg(securityLevel.name, requestSeed));
    securityLevel.requestSeed = quint8(value);

    securityLevel.seedLength = integer("seedLength", securityLevel.seedLength, 1);
    securityLevel.keyLength = integer("keyLength", securityLevel.keyLength, 1);
    securityLevel.maxAttempts = integer("maxAttempts", securityLevel.maxAttempts, 0);

    while (!reader.hasError() && reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Comment")) {
            securityLevel.comment = reader.readElementText();
        } else {
            if (reader.name() == QLatin1String("AllowedSession"))
                sessionNames->append(reader.attributes().value("name").toString());
            reader.skipCurrentElement();
        }
    }
    return securityLevel;
}

QString numberToXml(double value)
{
    return QLocale::c().toString(value, 'g', QLocale::FloatingPointShortest);
}

// The words used in the file for each enum value of the network settings.
const QVector<QPair<QString, Network::Protocol>> ProtocolWords = {
    {"can", Network::Protocol::Can}, {"canfd", Network::Protocol::CanFd},
    {"uart", Network::Protocol::Uart}, {"udp", Network::Protocol::Udp}};
const QVector<QPair<QString, IdFormat>> IdFormatWords = {
    {"standard", IdFormat::Standard}, {"extended", IdFormat::Extended}};
const QVector<QPair<QString, UartSettings::Parity>> ParityWords = {
    {"none", UartSettings::Parity::None}, {"even", UartSettings::Parity::Even},
    {"odd", UartSettings::Parity::Odd}, {"mark", UartSettings::Parity::Mark},
    {"space", UartSettings::Parity::Space}};
const QVector<QPair<QString, UartSettings::StopBits>> StopBitsWords = {
    {"1", UartSettings::StopBits::One}, {"1.5", UartSettings::StopBits::OneAndHalf},
    {"2", UartSettings::StopBits::Two}};
const QVector<QPair<QString, UartSettings::FlowControl>> FlowControlWords = {
    {"none", UartSettings::FlowControl::None}, {"hardware", UartSettings::FlowControl::Hardware},
    {"software", UartSettings::FlowControl::Software}};
const QVector<QPair<QString, UartSettings::Crc>> CrcWords = {
    {"crc8", UartSettings::Crc::Crc8}, {"crc16", UartSettings::Crc::Crc16}, {"crc32", UartSettings::Crc::Crc32}};
const QVector<QPair<QString, UdpSettings::Mode>> UdpModeWords = {
    {"unicast", UdpSettings::Mode::Unicast}, {"broadcast", UdpSettings::Mode::Broadcast},
    {"multicast", UdpSettings::Mode::Multicast}};

template <typename Enum>
QString enumToXml(Enum value, const QVector<QPair<QString, Enum>> &words)
{
    for (const auto &word : words) {
        if (word.second == value)
            return word.first;
    }
    return QString();
}

// Reads the attributes of one element of the <Network> section. A missing
// attribute keeps the value that is already there; a bad one raises an error.
class AttributeReader
{
public:
    AttributeReader(QXmlStreamReader &reader, const QString &element)
        : m_reader(reader), m_attributes(reader.attributes()), m_element(element) {}

    void read(const QString &name, int *value, int minimum, int maximum)
    {
        if (!m_attributes.hasAttribute(name))
            return;
        bool ok = false;
        const int number = m_attributes.value(name).toInt(&ok);
        if (!ok || number < minimum || number > maximum)
            fail(name, QString("a whole number from %1 to %2").arg(minimum).arg(maximum));
        else
            *value = number;
    }

    void read(const QString &name, double *value)
    {
        if (!m_attributes.hasAttribute(name))
            return;
        bool ok = false;
        const double number = QLocale::c().toDouble(m_attributes.value(name).toString(), &ok);
        if (!ok)
            fail(name, "a number");
        else
            *value = number;
    }

    void read(const QString &name, bool *value)
    {
        if (!m_attributes.hasAttribute(name))
            return;
        const QString text = m_attributes.value(name).toString();
        if (text != "true" && text != "false")
            fail(name, "true or false");
        else
            *value = (text == "true");
    }

    // IDs are written as hex ("0x7E0"); plain decimal is accepted too.
    void read(const QString &name, quint32 *value)
    {
        if (!m_attributes.hasAttribute(name))
            return;
        const QString text = m_attributes.value(name).toString().trimmed();
        bool ok = false;
        const quint32 number = text.startsWith("0x", Qt::CaseInsensitive) ? text.mid(2).toUInt(&ok, 16)
                                                                          : text.toUInt(&ok, 10);
        if (!ok)
            fail(name, "a number, hex with 0x in front (0x7E0) or decimal");
        else
            *value = number;
    }

    void read(const QString &name, QString *value)
    {
        if (m_attributes.hasAttribute(name))
            *value = m_attributes.value(name).toString().trimmed();
    }

    template <typename Enum>
    void read(const QString &name, Enum *value, const QVector<QPair<QString, Enum>> &words)
    {
        if (!m_attributes.hasAttribute(name))
            return;
        const QString text = m_attributes.value(name).toString();
        for (const auto &word : words) {
            if (word.first == text) {
                *value = word.second;
                return;
            }
        }
        QStringList allowed;
        for (const auto &word : words)
            allowed << word.first;
        fail(name, "one of: " + allowed.join(", "));
    }

private:
    void fail(const QString &name, const QString &expected)
    {
        if (!m_reader.hasError())
            m_reader.raiseError(QString("<%1>: '%2' must be %3.").arg(m_element, name, expected));
    }

    QXmlStreamReader &m_reader;
    QXmlStreamAttributes m_attributes;
    QString m_element;
};

// Reads the <Network> element: the protocol, the request/response IDs and the
// settings of that protocol. Settings that are missing
// keep their defaults.
Network readNetwork(QXmlStreamReader &reader)
{
    Network network;
    AttributeReader networkAttributes(reader, "Network");
    networkAttributes.read("protocol", &network.protocol, ProtocolWords);
    networkAttributes.read("requestId", &network.requestId);
    networkAttributes.read("responseId", &network.responseId);

    while (!reader.hasError() && reader.readNextStartElement()) {
        const QString element = reader.name().toString();
        AttributeReader attributes(reader, element);

        // Only the element of the active protocol is used.
        if (element == "Can" && network.protocol == Network::Protocol::Can) {
            attributes.read("bitrate", &network.can.bitrate, 1, 1000000);
            attributes.read("samplePoint", &network.can.samplePoint);
            attributes.read("idFormat", &network.can.idFormat, IdFormatWords);
        } else if (element == "CanFd" && network.protocol == Network::Protocol::CanFd) {
            attributes.read("nominalBitrate", &network.canFd.nominalBitrate, 1, 1000000);
            attributes.read("nominalSamplePoint", &network.canFd.nominalSamplePoint);
            attributes.read("dataBitrate", &network.canFd.dataBitrate, 1, 15000000);
            attributes.read("dataSamplePoint", &network.canFd.dataSamplePoint);
            attributes.read("iso", &network.canFd.iso);
            attributes.read("idFormat", &network.canFd.idFormat, IdFormatWords);
        } else if (element == "Uart" && network.protocol == Network::Protocol::Uart) {
            attributes.read("baudRate", &network.uart.baudRate, 1, 100000000);
            attributes.read("dataBits", &network.uart.dataBits, 5, 8);
            attributes.read("parity", &network.uart.parity, ParityWords);
            attributes.read("stopBits", &network.uart.stopBits, StopBitsWords);
            attributes.read("flowControl", &network.uart.flowControl, FlowControlWords);
            attributes.read("idSize", &network.uart.idSize, 1, 4);
            attributes.read("crc", &network.uart.crc, CrcWords);
        } else if (element == "Udp" && network.protocol == Network::Protocol::Udp) {
            attributes.read("mode", &network.udp.mode, UdpModeWords);
            attributes.read("localAddress", &network.udp.localAddress);
            attributes.read("localPort", &network.udp.localPort, 0, 65535);
            attributes.read("remoteAddress", &network.udp.remoteAddress);
            attributes.read("remotePort", &network.udp.remotePort, 0, 65535);
            attributes.read("multicastGroup", &network.udp.multicastGroup);
            attributes.read("idSize", &network.udp.idSize, 1, 4);
        }
        if (!reader.hasError())
            reader.skipCurrentElement();
    }

    // The IDs can only be checked now that the protocol's settings are known.
    if (!reader.hasError()) {
        const quint32 largestId = maxId(network);
        if (network.requestId > largestId || network.responseId > largestId)
            reader.raiseError(QString("<Network>: the request and response IDs must be at most %1 on %2.")
                                  .arg(idToText(largestId), toString(network.protocol)));
        else if (network.requestId == network.responseId)
            reader.raiseError("<Network>: the request and response IDs must be different.");
    }
    return network;
}

// Writes the protocol and only that protocol's settings.
void writeNetwork(QXmlStreamWriter &writer, const Network &network)
{
    writer.writeStartElement("Network");
    writer.writeAttribute("protocol", enumToXml(network.protocol, ProtocolWords));
    writer.writeAttribute("requestId", idToText(network.requestId));
    writer.writeAttribute("responseId", idToText(network.responseId));

    switch (network.protocol) {
    case Network::Protocol::Can:
        writer.writeEmptyElement("Can");
        writer.writeAttribute("bitrate", QString::number(network.can.bitrate));
        writer.writeAttribute("samplePoint", numberToXml(network.can.samplePoint));
        writer.writeAttribute("idFormat", enumToXml(network.can.idFormat, IdFormatWords));
        break;
    case Network::Protocol::CanFd:
        writer.writeEmptyElement("CanFd");
        writer.writeAttribute("nominalBitrate", QString::number(network.canFd.nominalBitrate));
        writer.writeAttribute("nominalSamplePoint", numberToXml(network.canFd.nominalSamplePoint));
        writer.writeAttribute("dataBitrate", QString::number(network.canFd.dataBitrate));
        writer.writeAttribute("dataSamplePoint", numberToXml(network.canFd.dataSamplePoint));
        writer.writeAttribute("iso", network.canFd.iso ? "true" : "false");
        writer.writeAttribute("idFormat", enumToXml(network.canFd.idFormat, IdFormatWords));
        break;
    case Network::Protocol::Uart:
        writer.writeEmptyElement("Uart");
        writer.writeAttribute("baudRate", QString::number(network.uart.baudRate));
        writer.writeAttribute("dataBits", QString::number(network.uart.dataBits));
        writer.writeAttribute("parity", enumToXml(network.uart.parity, ParityWords));
        writer.writeAttribute("stopBits", enumToXml(network.uart.stopBits, StopBitsWords));
        writer.writeAttribute("flowControl", enumToXml(network.uart.flowControl, FlowControlWords));
        writer.writeAttribute("idSize", QString::number(network.uart.idSize));
        writer.writeAttribute("crc", enumToXml(network.uart.crc, CrcWords));
        break;
    case Network::Protocol::Udp:
        writer.writeEmptyElement("Udp");
        writer.writeAttribute("mode", enumToXml(network.udp.mode, UdpModeWords));
        writer.writeAttribute("localAddress", network.udp.localAddress);
        writer.writeAttribute("localPort", QString::number(network.udp.localPort));
        writer.writeAttribute("remoteAddress", network.udp.remoteAddress);
        writer.writeAttribute("remotePort", QString::number(network.udp.remotePort));
        writer.writeAttribute("multicastGroup", network.udp.multicastGroup);
        writer.writeAttribute("idSize", QString::number(network.udp.idSize));
        break;
    }

    writer.writeEndElement(); // Network
}

// Reads one <ResetType> element. On a problem it calls reader.raiseError().
// The names of its allowed sessions and security levels go to *sessionNames and
// *securityLevelNames; they are turned into ids once the whole file is read.
ResetType readResetType(QXmlStreamReader &reader, QStringList *sessionNames, QStringList *securityLevelNames)
{
    const QXmlStreamAttributes attributes = reader.attributes();
    ResetType resetType;

    resetType.name = attributes.value("name").toString().trimmed();
    if (resetType.name.isEmpty())
        reader.raiseError("A reset type has no name.");

    // resetType is written as hex ("0x01"); plain decimal is accepted too.
    const QString type = attributes.value("resetType").toString().trimmed();
    bool ok = false;
    uint value = 0;
    if (type.startsWith("0x", Qt::CaseInsensitive))
        value = type.mid(2).toUInt(&ok, 16);
    else
        value = type.toUInt(&ok, 10);
    if (!ok || value > 0xFF || !isValidResetType(quint8(value)))
        reader.raiseError(QString("Reset type '%1': resetType '%2' is not a valid sub-function.")
                              .arg(resetType.name, type));
    resetType.resetType = quint8(value);

    // Only EnableRapidPowerShutDown reports a power down time; elsewhere it is ignored.
    if (resetType.resetType == EnableRapidPowerShutDown && attributes.hasAttribute("powerDownTime")) {
        const int powerDownTime = attributes.value("powerDownTime").toInt(&ok);
        if (!ok || powerDownTime < 0 || powerDownTime > PowerDownTimeNotAvailable)
            reader.raiseError(QString("Reset type '%1': 'powerDownTime' must be a whole number from 0 to %2.")
                                  .arg(resetType.name).arg(PowerDownTimeNotAvailable));
        else
            resetType.powerDownTime = powerDownTime;
    }

    while (!reader.hasError() && reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Comment")) {
            resetType.comment = reader.readElementText();
        } else {
            if (reader.name() == QLatin1String("AllowedSession"))
                sessionNames->append(reader.attributes().value("name").toString());
            else if (reader.name() == QLatin1String("SecurityLevel"))
                securityLevelNames->append(reader.attributes().value("name").toString());
            reader.skipCurrentElement();
        }
    }
    return resetType;
}

// Turns the names in `names` into ids by `idByName` and puts them in *ids (each once),
// in document order, which is the order the dialogs list them in; then an edit that
// changes nothing compares equal. If a name is unknown, puts what is wrong in
// *errorMessage and returns false.
bool resolveNames(const QStringList &names, const QHash<QString, int> &idByName, QVector<int> *ids,
                  const QString &owner, const QString &kind, QString *errorMessage)
{
    for (const QString &name : names) {
        if (!idByName.contains(name)) {
            *errorMessage = QString("%1 refers to %2 '%3', which does not exist.").arg(owner, kind, name);
            return false;
        }
        const int id = idByName.value(name);
        if (!ids->contains(id))
            ids->append(id);
    }
    // Ids are given in document order, so ascending ids are document order.
    std::sort(ids->begin(), ids->end());
    return true;
}

} // namespace

namespace XmlStorage {

bool save(const Database &database, const QString &filePath, QString *errorMessage)
{
    // QSaveFile writes to a temporary file and only replaces the real file at
    // commit(), so a failed save never leaves a half-written file behind.
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        *errorMessage = file.errorString();
        return false;
    }

    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement("UDScribe");
    writer.writeAttribute("version", FormatVersion);

    writeNetwork(writer, database.network());

    writer.writeStartElement("ValueTables");
    for (const ValueTable &valueTable : database.allValueTables()) {
        writer.writeStartElement("ValueTable");
        writer.writeAttribute("name", valueTable.name);
        for (const ValueTable::Entry &entry : valueTable.entries) {
            writer.writeEmptyElement("Value");
            writer.writeAttribute("value", QString::number(entry.value));
            writer.writeAttribute("description", entry.description);
        }
        if (!valueTable.comment.isEmpty())
            writer.writeTextElement("Comment", valueTable.comment);
        writer.writeEndElement(); // ValueTable
    }
    writer.writeEndElement(); // ValueTables

    writer.writeStartElement("DataTypes");
    for (const DataType &dataType : database.allDataTypes()) {
        writer.writeStartElement("DataType");
        writer.writeAttribute("name", dataType.name);
        writer.writeAttribute("length", QString::number(dataType.length));
        writer.writeAttribute("byteOrder", byteOrderToXml(dataType.byteOrder));
        writer.writeAttribute("valueType", valueTypeToXml(dataType.valueType));
        writer.writeAttribute("unit", dataType.unit);
        writer.writeAttribute("factor", numberToXml(dataType.factor));
        writer.writeAttribute("offset", numberToXml(dataType.offset));
        writer.writeAttribute("minimum", numberToXml(dataType.minimum));
        writer.writeAttribute("maximum", numberToXml(dataType.maximum));
        // The value table is referred to by name: ids are not saved.
        if (const ValueTable *valueTable = database.findValueTable(dataType.valueTableId))
            writer.writeAttribute("valueTable", valueTable->name);
        if (!dataType.comment.isEmpty())
            writer.writeTextElement("Comment", dataType.comment);
        writer.writeEndElement(); // DataType
    }
    writer.writeEndElement(); // DataTypes

    writer.writeStartElement("DataIdentifiers");
    for (const DataIdentifier &dataIdentifier : database.allDataIdentifiers()) {
        writer.writeStartElement("DataIdentifier");
        writer.writeAttribute("name", dataIdentifier.name);
        writer.writeAttribute("did", didToText(dataIdentifier.did));
        writer.writeAttribute("length", QString::number(dataIdentifier.length));
        // Data types, sessions and levels are referred to by name: ids are not saved.
        for (const DataTypeMapping &mapping : dataIdentifier.mappings) {
            const DataType *dataType = database.findDataType(mapping.dataTypeId);
            if (!dataType)
                continue;
            writer.writeEmptyElement("DataType");
            writer.writeAttribute("name", mapping.name);
            writer.writeAttribute("dataType", dataType->name);
            writer.writeAttribute("startBit", QString::number(mapping.startBit));
        }
        for (int sessionId : dataIdentifier.readSessionIds) {
            if (const Session *session = database.findSession(sessionId)) {
                writer.writeEmptyElement("ReadSession");
                writer.writeAttribute("name", session->name);
            }
        }
        for (int securityLevelId : dataIdentifier.readSecurityLevelIds) {
            if (const SecurityLevel *securityLevel = database.findSecurityLevel(securityLevelId)) {
                writer.writeEmptyElement("ReadSecurityLevel");
                writer.writeAttribute("name", securityLevel->name);
            }
        }
        if (!dataIdentifier.comment.isEmpty())
            writer.writeTextElement("Comment", dataIdentifier.comment);
        writer.writeEndElement(); // DataIdentifier
    }
    writer.writeEndElement(); // DataIdentifiers

    writer.writeStartElement("Sessions");
    for (const Session &session : database.allSessions()) {
        writer.writeStartElement("Session");
        writer.writeAttribute("name", session.name);
        writer.writeAttribute("sessionType", subFunctionToText(session.sessionType));
        writer.writeAttribute("p2", QString::number(session.p2));
        writer.writeAttribute("p2Star", QString::number(session.p2Star));
        if (!session.comment.isEmpty())
            writer.writeTextElement("Comment", session.comment);
        writer.writeEndElement(); // Session
    }
    writer.writeEndElement(); // Sessions

    writer.writeStartElement("SecurityLevels");
    for (const SecurityLevel &securityLevel : database.allSecurityLevels()) {
        writer.writeStartElement("SecurityLevel");
        writer.writeAttribute("name", securityLevel.name);
        writer.writeAttribute("requestSeed", subFunctionToText(securityLevel.requestSeed));
        writer.writeAttribute("seedLength", QString::number(securityLevel.seedLength));
        writer.writeAttribute("keyLength", QString::number(securityLevel.keyLength));
        writer.writeAttribute("maxAttempts", QString::number(securityLevel.maxAttempts));
        for (int sessionId : securityLevel.sessionIds) {
            if (const Session *session = database.findSession(sessionId)) {
                writer.writeEmptyElement("AllowedSession");
                writer.writeAttribute("name", session->name);
            }
        }
        if (!securityLevel.comment.isEmpty())
            writer.writeTextElement("Comment", securityLevel.comment);
        writer.writeEndElement(); // SecurityLevel
    }
    writer.writeEndElement(); // SecurityLevels

    writer.writeStartElement("ResetTypes");
    for (const ResetType &resetType : database.allResetTypes()) {
        writer.writeStartElement("ResetType");
        writer.writeAttribute("name", resetType.name);
        writer.writeAttribute("resetType", subFunctionToText(resetType.resetType));
        if (resetType.resetType == EnableRapidPowerShutDown)
            writer.writeAttribute("powerDownTime", QString::number(resetType.powerDownTime));
        for (int sessionId : resetType.sessionIds) {
            if (const Session *session = database.findSession(sessionId)) {
                writer.writeEmptyElement("AllowedSession");
                writer.writeAttribute("name", session->name);
            }
        }
        for (int securityLevelId : resetType.securityLevelIds) {
            if (const SecurityLevel *securityLevel = database.findSecurityLevel(securityLevelId)) {
                writer.writeEmptyElement("SecurityLevel");
                writer.writeAttribute("name", securityLevel->name);
            }
        }
        if (!resetType.comment.isEmpty())
            writer.writeTextElement("Comment", resetType.comment);
        writer.writeEndElement(); // ResetType
    }
    writer.writeEndElement(); // ResetTypes

    writer.writeEndElement(); // UDScribe
    writer.writeEndDocument();

    if (writer.hasError() || !file.commit()) {
        *errorMessage = file.errorString();
        return false;
    }
    return true;
}

bool load(Database &database, const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        *errorMessage = file.errorString();
        return false;
    }

    QXmlStreamReader reader(&file);
    Network loadedNetwork; // the defaults (CAN), if the file has no <Network>
    QVector<ValueTable> loadedValueTables;
    QSet<QString> valueTableNames;
    QVector<DataType> loadedDataTypes;
    QStringList dataTypeValueTableNames; // per loaded data type; resolved once all tables are read
    QSet<QString> dataTypeNames;
    QVector<DataIdentifier> loadedDataIdentifiers;
    QVector<DataIdentifierRefs> dataIdentifierRefs; // per loaded DID
    QSet<QString> dataIdentifierNames;
    QSet<quint16> dids;
    QVector<Session> loadedSessions; // the default session is added if missing (old files)
    QSet<QString> sessionNames;
    QSet<quint8> sessionTypes;
    QVector<SecurityLevel> loadedSecurityLevels;
    QVector<QStringList> allowedSessionNames; // per loaded security level; resolved once all sessions are read
    QSet<QString> securityLevelNames;
    QVector<ResetType> loadedResetTypes;
    QVector<QStringList> resetTypeSessionNames;       // per loaded reset type, resolved at the end
    QVector<QStringList> resetTypeSecurityLevelNames; // likewise
    QSet<QString> resetTypeNames;
    QSet<quint8> resetTypeValues;
    QSet<quint8> requestSeeds;

    if (!reader.readNextStartElement() || reader.name() != QLatin1String("UDScribe")) {
        if (!reader.hasError())
            reader.raiseError("This is not a UDScribe file.");
    } else if (reader.attributes().value("version") != FormatVersion) {
        reader.raiseError(QString("Unsupported file version '%1'.")
                              .arg(reader.attributes().value("version").toString()));
    }

    // Sections we do not know (e.g. from a newer version) are skipped.
    while (!reader.hasError() && reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Network")) {
            loadedNetwork = readNetwork(reader);
        } else if (reader.name() == QLatin1String("ValueTables")) {
            while (!reader.hasError() && reader.readNextStartElement()) {
                if (reader.name() != QLatin1String("ValueTable")) {
                    reader.skipCurrentElement();
                    continue;
                }
                ValueTable valueTable = readValueTable(reader);
                valueTable.id = loadedValueTables.size() + 1; // temporary; Database renumbers
                if (!reader.hasError() && valueTableNames.contains(valueTable.name))
                    reader.raiseError(QString("There are two value tables named '%1'.").arg(valueTable.name));
                valueTableNames.insert(valueTable.name);
                loadedValueTables.append(valueTable);
            }
        } else if (reader.name() == QLatin1String("DataTypes")) {
            while (!reader.hasError() && reader.readNextStartElement()) {
                if (reader.name() != QLatin1String("DataType")) {
                    reader.skipCurrentElement();
                    continue;
                }
                dataTypeValueTableNames << reader.attributes().value("valueTable").toString();
                DataType dataType = readDataType(reader);
                dataType.id = loadedDataTypes.size() + 1; // temporary; Database renumbers
                if (!reader.hasError() && dataTypeNames.contains(dataType.name))
                    reader.raiseError(QString("There are two data types named '%1'.").arg(dataType.name));
                dataTypeNames.insert(dataType.name);
                loadedDataTypes.append(dataType);
            }
        } else if (reader.name() == QLatin1String("DataIdentifiers")) {
            while (!reader.hasError() && reader.readNextStartElement()) {
                if (reader.name() != QLatin1String("DataIdentifier")) {
                    reader.skipCurrentElement();
                    continue;
                }
                DataIdentifierRefs refs;
                const DataIdentifier dataIdentifier = readDataIdentifier(reader, &refs);
                if (!reader.hasError() && dataIdentifierNames.contains(dataIdentifier.name))
                    reader.raiseError(QString("There are two DIDs named '%1'.").arg(dataIdentifier.name));
                if (!reader.hasError() && dids.contains(dataIdentifier.did))
                    reader.raiseError(QString("There are two DIDs with did %1.").arg(didToText(dataIdentifier.did)));
                dataIdentifierNames.insert(dataIdentifier.name);
                dids.insert(dataIdentifier.did);
                loadedDataIdentifiers.append(dataIdentifier);
                dataIdentifierRefs.append(refs);
            }
        } else if (reader.name() == QLatin1String("Sessions")) {
            while (!reader.hasError() && reader.readNextStartElement()) {
                if (reader.name() != QLatin1String("Session")) {
                    reader.skipCurrentElement();
                    continue;
                }
                Session session = readSession(reader);
                session.id = loadedSessions.size() + 1; // temporary; Database renumbers
                if (!reader.hasError() && sessionNames.contains(session.name))
                    reader.raiseError(QString("There are two sessions named '%1'.").arg(session.name));
                if (!reader.hasError() && sessionTypes.contains(session.sessionType))
                    reader.raiseError(QString("There are two sessions with sessionType %1.")
                                          .arg(subFunctionToText(session.sessionType)));
                sessionNames.insert(session.name);
                sessionTypes.insert(session.sessionType);
                loadedSessions.append(session);
            }
        } else if (reader.name() == QLatin1String("SecurityLevels")) {
            while (!reader.hasError() && reader.readNextStartElement()) {
                if (reader.name() != QLatin1String("SecurityLevel")) {
                    reader.skipCurrentElement();
                    continue;
                }
                QStringList sessionNames;
                SecurityLevel securityLevel = readSecurityLevel(reader, &sessionNames);
                securityLevel.id = loadedSecurityLevels.size() + 1; // temporary; Database renumbers
                if (!reader.hasError() && securityLevelNames.contains(securityLevel.name))
                    reader.raiseError(QString("There are two security levels named '%1'.").arg(securityLevel.name));
                if (!reader.hasError() && requestSeeds.contains(securityLevel.requestSeed))
                    reader.raiseError(QString("There are two security levels with requestSeed %1.")
                                          .arg(subFunctionToText(securityLevel.requestSeed)));
                securityLevelNames.insert(securityLevel.name);
                requestSeeds.insert(securityLevel.requestSeed);
                loadedSecurityLevels.append(securityLevel);
                allowedSessionNames.append(sessionNames);
            }
        } else if (reader.name() == QLatin1String("ResetTypes")) {
            while (!reader.hasError() && reader.readNextStartElement()) {
                if (reader.name() != QLatin1String("ResetType")) {
                    reader.skipCurrentElement();
                    continue;
                }
                QStringList sessionNames;
                QStringList securityLevelNames;
                const ResetType resetType = readResetType(reader, &sessionNames, &securityLevelNames);
                if (!reader.hasError() && resetTypeNames.contains(resetType.name))
                    reader.raiseError(QString("There are two reset types named '%1'.").arg(resetType.name));
                if (!reader.hasError() && resetTypeValues.contains(resetType.resetType))
                    reader.raiseError(QString("There are two reset types with resetType %1.")
                                          .arg(subFunctionToText(resetType.resetType)));
                resetTypeNames.insert(resetType.name);
                resetTypeValues.insert(resetType.resetType);
                loadedResetTypes.append(resetType);
                resetTypeSessionNames.append(sessionNames);
                resetTypeSecurityLevelNames.append(securityLevelNames);
            }
        } else {
            reader.skipCurrentElement();
        }
    }

    if (reader.hasError()) {
        *errorMessage = QString("Line %1: %2").arg(reader.lineNumber()).arg(reader.errorString());
        return false;
    }

    // Now that all value tables are known, turn the table names in data types into ids.
    QHash<QString, int> valueTableIdByName;
    for (const ValueTable &valueTable : loadedValueTables)
        valueTableIdByName.insert(valueTable.name, valueTable.id);
    for (int i = 0; i < loadedDataTypes.size(); ++i) {
        const QString &name = dataTypeValueTableNames.at(i);
        if (name.isEmpty())
            continue;
        if (!valueTableIdByName.contains(name)) {
            *errorMessage = QString("Data type '%1' uses value table '%2', which does not exist.")
                                .arg(loadedDataTypes.at(i).name, name);
            return false;
        }
        loadedDataTypes[i].valueTableId = valueTableIdByName.value(name);
    }

    // Now that the whole file is read, turn the session and security level names into ids.
    QHash<QString, int> sessionIdByName;
    for (const Session &session : loadedSessions)
        sessionIdByName.insert(session.name, session.id);
    QHash<QString, int> securityLevelIdByName;
    for (const SecurityLevel &securityLevel : loadedSecurityLevels)
        securityLevelIdByName.insert(securityLevel.name, securityLevel.id);
    for (int i = 0; i < loadedSecurityLevels.size(); ++i) {
        SecurityLevel &securityLevel = loadedSecurityLevels[i];
        const QString owner = QString("Security level '%1'").arg(securityLevel.name);
        if (!resolveNames(allowedSessionNames.at(i), sessionIdByName, &securityLevel.sessionIds, owner, "session",
                          errorMessage))
            return false;
    }
    for (int i = 0; i < loadedResetTypes.size(); ++i) {
        ResetType &resetType = loadedResetTypes[i];
        const QString owner = QString("Reset type '%1'").arg(resetType.name);
        if (!resolveNames(resetTypeSessionNames.at(i), sessionIdByName, &resetType.sessionIds, owner, "session",
                          errorMessage)
            || !resolveNames(resetTypeSecurityLevelNames.at(i), securityLevelIdByName, &resetType.securityLevelIds,
                             owner, "security level", errorMessage))
            return false;
    }

    // DIDs: their data types, sessions and levels, then whether the data types fit.
    QHash<QString, int> dataTypeIdByName;
    QHash<int, DataType> dataTypesById;
    for (const DataType &dataType : loadedDataTypes) {
        dataTypeIdByName.insert(dataType.name, dataType.id);
        dataTypesById.insert(dataType.id, dataType);
    }
    for (int i = 0; i < loadedDataIdentifiers.size(); ++i) {
        DataIdentifier &dataIdentifier = loadedDataIdentifiers[i];
        const DataIdentifierRefs &refs = dataIdentifierRefs.at(i);
        const QString owner = QString("DID %1 '%2'").arg(didToText(dataIdentifier.did), dataIdentifier.name);
        for (int m = 0; m < dataIdentifier.mappings.size(); ++m) {
            const QString &dataTypeName = refs.dataTypes.at(m);
            if (!dataTypeIdByName.contains(dataTypeName)) {
                *errorMessage = QString("%1 refers to data type '%2', which does not exist.").arg(owner, dataTypeName);
                return false;
            }
            dataIdentifier.mappings[m].dataTypeId = dataTypeIdByName.value(dataTypeName);
        }
        if (!resolveNames(refs.readSessions, sessionIdByName, &dataIdentifier.readSessionIds, owner, "session",
                          errorMessage)
            || !resolveNames(refs.readSecurityLevels, securityLevelIdByName, &dataIdentifier.readSecurityLevelIds,
                             owner, "security level", errorMessage))
            return false;
        const QString layoutError = checkDataTypeLayout(dataIdentifier.length, dataIdentifier.mappings, dataTypesById);
        if (!layoutError.isEmpty()) {
            *errorMessage = QString("%1: %2").arg(owner, layoutError);
            return false;
        }
    }

    database.replaceAll(loadedNetwork, loadedSessions, loadedSecurityLevels, loadedResetTypes, loadedValueTables,
                        loadedDataTypes, loadedDataIdentifiers);
    return true;
}

} // namespace XmlStorage

} // namespace usc
