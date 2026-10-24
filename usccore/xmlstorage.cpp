#include "xmlstorage.h"
#include "database.h"

#include <QFile>
#include <QLocale>
#include <QSaveFile>
#include <QSet>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace usc {

namespace {

const QString FormatVersion = "1";

// Reads one <SecurityLevel> element. On a problem it calls reader.raiseError().
SecurityLevel readSecurityLevel(QXmlStreamReader &reader)
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
        if (reader.name() == QLatin1String("Comment"))
            securityLevel.comment = reader.readElementText();
        else
            reader.skipCurrentElement();
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

// Reads the <Network> element: the protocol, the request/response IDs, the
// response timing and the settings of that protocol. Settings that are missing
// keep their defaults.
Network readNetwork(QXmlStreamReader &reader)
{
    Network network;
    AttributeReader networkAttributes(reader, "Network");
    networkAttributes.read("protocol", &network.protocol, ProtocolWords);
    networkAttributes.read("requestId", &network.requestId);
    networkAttributes.read("responseId", &network.responseId);
    // The largest values a DiagnosticSessionControl response can report:
    // P2 in 1 ms steps, P2* in 10 ms steps, both as 16-bit numbers.
    networkAttributes.read("p2", &network.p2, 0, 65535);
    networkAttributes.read("p2Star", &network.p2Star, 0, 655350);

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
    writer.writeAttribute("p2", QString::number(network.p2));
    writer.writeAttribute("p2Star", QString::number(network.p2Star));

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

    writer.writeStartElement("SecurityLevels");
    for (const SecurityLevel &securityLevel : database.allSecurityLevels()) {
        writer.writeStartElement("SecurityLevel");
        writer.writeAttribute("name", securityLevel.name);
        writer.writeAttribute("requestSeed", subFunctionToText(securityLevel.requestSeed));
        writer.writeAttribute("seedLength", QString::number(securityLevel.seedLength));
        writer.writeAttribute("keyLength", QString::number(securityLevel.keyLength));
        writer.writeAttribute("maxAttempts", QString::number(securityLevel.maxAttempts));
        if (!securityLevel.comment.isEmpty())
            writer.writeTextElement("Comment", securityLevel.comment);
        writer.writeEndElement(); // SecurityLevel
    }
    writer.writeEndElement(); // SecurityLevels

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
    QVector<SecurityLevel> loadedSecurityLevels;
    QSet<QString> securityLevelNames;
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
        } else if (reader.name() == QLatin1String("SecurityLevels")) {
            while (!reader.hasError() && reader.readNextStartElement()) {
                if (reader.name() != QLatin1String("SecurityLevel")) {
                    reader.skipCurrentElement();
                    continue;
                }
                const SecurityLevel securityLevel = readSecurityLevel(reader);
                if (!reader.hasError() && securityLevelNames.contains(securityLevel.name))
                    reader.raiseError(QString("There are two security levels named '%1'.").arg(securityLevel.name));
                if (!reader.hasError() && requestSeeds.contains(securityLevel.requestSeed))
                    reader.raiseError(QString("There are two security levels with requestSeed %1.")
                                          .arg(subFunctionToText(securityLevel.requestSeed)));
                securityLevelNames.insert(securityLevel.name);
                requestSeeds.insert(securityLevel.requestSeed);
                loadedSecurityLevels.append(securityLevel);
            }
        } else {
            reader.skipCurrentElement();
        }
    }

    if (reader.hasError()) {
        *errorMessage = QString("Line %1: %2").arg(reader.lineNumber()).arg(reader.errorString());
        return false;
    }

    database.replaceAll(loadedNetwork, loadedSecurityLevels);
    return true;
}

} // namespace XmlStorage

} // namespace usc
