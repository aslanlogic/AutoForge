#include "xmlstorage.h"
#include "database.h"

#include <QFile>
#include <QHash>
#include <QLocale>
#include <QSaveFile>
#include <QSet>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace {

const QString FormatVersion = "1";

// Words used in the file. They are kept separate from the texts shown in the
// GUI, so renaming a GUI label never breaks existing files.
QString byteOrderToXml(Signal::ByteOrder byteOrder)
{
    switch (byteOrder) {
    case Signal::ByteOrder::Intel:    return "intel";
    case Signal::ByteOrder::Motorola: return "motorola";
    }
    return QString();
}

QString valueTypeToXml(Signal::ValueType valueType)
{
    switch (valueType) {
    case Signal::ValueType::Signed:     return "signed";
    case Signal::ValueType::Unsigned:   return "unsigned";
    case Signal::ValueType::IeeeFloat:  return "float";
    case Signal::ValueType::IeeeDouble: return "double";
    }
    return QString();
}

QString numberToXml(double value)
{
    return QLocale::c().toString(value, 'g', QLocale::FloatingPointShortest);
}

// Reads one <Signal> element. On a problem it calls reader.raiseError().
Signal readSignal(QXmlStreamReader &reader)
{
    const QXmlStreamAttributes attributes = reader.attributes();
    const QLocale c = QLocale::c();
    Signal signal;

    auto number = [&](const QString &name, double defaultValue) {
        if (!attributes.hasAttribute(name))
            return defaultValue;
        bool ok = false;
        const double value = c.toDouble(attributes.value(name).toString(), &ok);
        if (!ok)
            reader.raiseError(QString("'%1' is not a number.").arg(name));
        return value;
    };

    signal.name = attributes.value("name").toString().trimmed();
    if (signal.name.isEmpty())
        reader.raiseError("A signal has no name.");

    const double length = number("length", signal.length);
    signal.length = int(length);
    if (signal.length != length || signal.length < 1 || signal.length > 64)
        reader.raiseError(QString("Signal '%1': length must be a whole number from 1 to 64.").arg(signal.name));

    const QString byteOrder = attributes.value("byteOrder").toString();
    if (byteOrder == "intel" || byteOrder.isEmpty())
        signal.byteOrder = Signal::ByteOrder::Intel;
    else if (byteOrder == "motorola")
        signal.byteOrder = Signal::ByteOrder::Motorola;
    else
        reader.raiseError(QString("Signal '%1': unknown byteOrder '%2'.").arg(signal.name, byteOrder));

    const QString valueType = attributes.value("valueType").toString();
    if (valueType == "signed" || valueType.isEmpty())
        signal.valueType = Signal::ValueType::Signed;
    else if (valueType == "unsigned")
        signal.valueType = Signal::ValueType::Unsigned;
    else if (valueType == "float")
        signal.valueType = Signal::ValueType::IeeeFloat;
    else if (valueType == "double")
        signal.valueType = Signal::ValueType::IeeeDouble;
    else
        reader.raiseError(QString("Signal '%1': unknown valueType '%2'.").arg(signal.name, valueType));

    signal.unit = attributes.value("unit").toString();
    signal.initialValue = number("initialValue", signal.initialValue);
    signal.factor = number("factor", signal.factor);
    signal.offset = number("offset", signal.offset);
    signal.minimum = number("minimum", signal.minimum);
    signal.maximum = number("maximum", signal.maximum);

    // Child elements; ones we do not know (e.g. from a newer version) are skipped.
    while (!reader.hasError() && reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Comment"))
            signal.comment = reader.readElementText();
        else
            reader.skipCurrentElement();
    }

    return signal;
}

QString messageTypeToXml(Message::Type type)
{
    switch (type) {
    case Message::Type::CanStandard:   return "can-standard";
    case Message::Type::CanExtended:   return "can-extended";
    case Message::Type::CanFdStandard: return "canfd-standard";
    case Message::Type::CanFdExtended: return "canfd-extended";
    case Message::Type::Uart:          return "uart";
    case Message::Type::Udp:           return "udp";
    }
    return QString();
}

QString txMethodToXml(Message::TxMethod txMethod)
{
    switch (txMethod) {
    case Message::TxMethod::Cyclic:           return "cyclic";
    case Message::TxMethod::OnEvent:          return "on-event";
    case Message::TxMethod::CyclicAndOnEvent: return "cyclic-and-on-event";
    }
    return QString();
}

// Reads one <Message> element. On a problem it calls reader.raiseError().
// Its <SignalRef> elements name signals that may not have been read yet, so
// they are returned in signalRefs (name, startBit) and resolved later.
Message readMessage(QXmlStreamReader &reader, QVector<QPair<QString, int>> *signalRefs)
{
    const QXmlStreamAttributes attributes = reader.attributes();
    Message message;

    auto integer = [&](const QString &name, int defaultValue) {
        if (!attributes.hasAttribute(name))
            return defaultValue;
        bool ok = false;
        const int value = attributes.value(name).toInt(&ok);
        if (!ok)
            reader.raiseError(QString("Message '%1': '%2' is not a whole number.").arg(message.name, name));
        return value;
    };

    message.name = attributes.value("name").toString().trimmed();
    if (message.name.isEmpty())
        reader.raiseError("A message has no name.");

    const QString type = attributes.value("type").toString();
    bool typeFound = type.isEmpty(); // missing means the default, CAN Standard
    for (auto candidate : {Message::Type::CanStandard, Message::Type::CanExtended,
                           Message::Type::CanFdStandard, Message::Type::CanFdExtended,
                           Message::Type::Uart, Message::Type::Udp}) {
        if (type == messageTypeToXml(candidate)) {
            message.type = candidate;
            typeFound = true;
        }
    }
    if (!typeFound)
        reader.raiseError(QString("Message '%1': unknown type '%2'.").arg(message.name, type));

    // frameId is written as hex ("0x1A"); plain decimal is accepted too.
    const QString frameId = attributes.value("frameId").toString().trimmed();
    bool ok = true;
    if (frameId.startsWith("0x", Qt::CaseInsensitive))
        message.frameId = frameId.mid(2).toUInt(&ok, 16);
    else if (!frameId.isEmpty())
        message.frameId = frameId.toUInt(&ok, 10);
    if (!ok || message.frameId > maxFrameId(message.type))
        reader.raiseError(QString("Message '%1': frameId '%2' is not valid for %3.")
                              .arg(message.name, frameId, toString(message.type)));

    message.length = integer("length", message.length);
    if (!isValidLength(message.type, message.length))
        reader.raiseError(QString("Message '%1': length %2 is not valid for %3.")
                              .arg(message.name).arg(message.length).arg(toString(message.type)));

    const QString txMethod = attributes.value("txMethod").toString();
    bool txMethodFound = txMethod.isEmpty(); // missing means the default, Cyclic
    for (auto candidate : {Message::TxMethod::Cyclic, Message::TxMethod::OnEvent,
                           Message::TxMethod::CyclicAndOnEvent}) {
        if (txMethod == txMethodToXml(candidate)) {
            message.txMethod = candidate;
            txMethodFound = true;
        }
    }
    if (!txMethodFound)
        reader.raiseError(QString("Message '%1': unknown txMethod '%2'.").arg(message.name, txMethod));

    message.cycleTime = integer("cycleTime", message.cycleTime);
    if (message.cycleTime < 1)
        reader.raiseError(QString("Message '%1': cycleTime must be at least 1 ms.").arg(message.name));

    while (!reader.hasError() && reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Comment")) {
            message.comment = reader.readElementText();
        } else if (reader.name() == QLatin1String("SignalRef")) {
            bool ok = false;
            const QString signalName = reader.attributes().value("name").toString();
            const int startBit = reader.attributes().value("startBit").toInt(&ok);
            if (!ok || startBit < 0)
                reader.raiseError(QString("Message '%1': signal '%2' has no valid startBit.")
                                      .arg(message.name, signalName));
            signalRefs->append({signalName, startBit});
            reader.skipCurrentElement();
        } else {
            reader.skipCurrentElement();
        }
    }

    return message;
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
    writer.writeStartElement("FrameForge");
    writer.writeAttribute("version", FormatVersion);

    writer.writeStartElement("Signals");
    for (const Signal &signal : database.allSignals()) {
        writer.writeStartElement("Signal");
        writer.writeAttribute("name", signal.name);
        writer.writeAttribute("length", QString::number(signal.length));
        writer.writeAttribute("byteOrder", byteOrderToXml(signal.byteOrder));
        writer.writeAttribute("valueType", valueTypeToXml(signal.valueType));
        writer.writeAttribute("unit", signal.unit);
        writer.writeAttribute("initialValue", numberToXml(signal.initialValue));
        writer.writeAttribute("factor", numberToXml(signal.factor));
        writer.writeAttribute("offset", numberToXml(signal.offset));
        writer.writeAttribute("minimum", numberToXml(signal.minimum));
        writer.writeAttribute("maximum", numberToXml(signal.maximum));
        if (!signal.comment.isEmpty())
            writer.writeTextElement("Comment", signal.comment);
        writer.writeEndElement(); // Signal
    }
    writer.writeEndElement(); // Signals

    writer.writeStartElement("Messages");
    for (const Message &message : database.allMessages()) {
        writer.writeStartElement("Message");
        writer.writeAttribute("name", message.name);
        writer.writeAttribute("type", messageTypeToXml(message.type));
        writer.writeAttribute("frameId", frameIdToText(message.frameId));
        writer.writeAttribute("length", QString::number(message.length));
        writer.writeAttribute("txMethod", txMethodToXml(message.txMethod));
        writer.writeAttribute("cycleTime", QString::number(message.cycleTime));
        // Signals are referred to by name: ids are not saved.
        for (const SignalMapping &mapping : message.signalMappings) {
            const Signal *signal = database.findSignal(mapping.signalId);
            if (!signal)
                continue;
            writer.writeEmptyElement("SignalRef");
            writer.writeAttribute("name", signal->name);
            writer.writeAttribute("startBit", QString::number(mapping.startBit));
        }
        if (!message.comment.isEmpty())
            writer.writeTextElement("Comment", message.comment);
        writer.writeEndElement(); // Message
    }
    writer.writeEndElement(); // Messages

    writer.writeEndElement(); // FrameForge
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
    QVector<Signal> loadedSignals;
    QVector<Message> loadedMessages;
    QSet<QString> signalNames;
    QSet<QString> messageNames;
    QVector<QVector<QPair<QString, int>>> signalRefs; // per loaded message

    if (!reader.readNextStartElement() || reader.name() != QLatin1String("FrameForge")) {
        if (!reader.hasError())
            reader.raiseError("This is not a FrameForge file.");
    } else if (reader.attributes().value("version") != FormatVersion) {
        reader.raiseError(QString("Unsupported file version '%1'.")
                              .arg(reader.attributes().value("version").toString()));
    }

    // Sections we do not know (e.g. from a newer version) are skipped.
    while (!reader.hasError() && reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Signals")) {
            while (!reader.hasError() && reader.readNextStartElement()) {
                if (reader.name() != QLatin1String("Signal")) {
                    reader.skipCurrentElement();
                    continue;
                }
                Signal signal = readSignal(reader);
                signal.id = loadedSignals.size() + 1; // temporary; Database renumbers
                if (!reader.hasError() && signalNames.contains(signal.name))
                    reader.raiseError(QString("There are two signals named '%1'.").arg(signal.name));
                signalNames.insert(signal.name);
                loadedSignals.append(signal);
            }
        } else if (reader.name() == QLatin1String("Messages")) {
            while (!reader.hasError() && reader.readNextStartElement()) {
                if (reader.name() != QLatin1String("Message")) {
                    reader.skipCurrentElement();
                    continue;
                }
                QVector<QPair<QString, int>> refs;
                const Message message = readMessage(reader, &refs);
                if (!reader.hasError() && messageNames.contains(message.name))
                    reader.raiseError(QString("There are two messages named '%1'.").arg(message.name));
                messageNames.insert(message.name);
                loadedMessages.append(message);
                signalRefs.append(refs);
            }
        } else {
            reader.skipCurrentElement();
        }
    }

    if (reader.hasError()) {
        *errorMessage = QString("Line %1: %2").arg(reader.lineNumber()).arg(reader.errorString());
        return false;
    }

    // Now that all signals are known, turn the signal names in messages into ids
    // and check that the signals fit in their message.
    QHash<QString, int> signalIdByName;
    QHash<int, Signal> signalsById;
    for (const Signal &signal : loadedSignals) {
        signalIdByName.insert(signal.name, signal.id);
        signalsById.insert(signal.id, signal);
    }
    for (int i = 0; i < loadedMessages.size(); ++i) {
        Message &message = loadedMessages[i];
        for (const auto &ref : signalRefs.at(i)) {
            if (!signalIdByName.contains(ref.first)) {
                *errorMessage = QString("Message '%1' uses signal '%2', which does not exist.")
                                    .arg(message.name, ref.first);
                return false;
            }
            message.signalMappings.append({signalIdByName.value(ref.first), ref.second});
        }
        const QString layoutError = checkSignalLayout(message.length, message.signalMappings, signalsById);
        if (!layoutError.isEmpty()) {
            *errorMessage = QString("Message '%1': %2").arg(message.name, layoutError);
            return false;
        }
    }

    database.replaceAll(loadedSignals, loadedMessages);
    return true;
}

} // namespace XmlStorage
