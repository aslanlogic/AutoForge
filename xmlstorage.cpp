#include "xmlstorage.h"
#include "database.h"

#include <QFile>
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
    QSet<QString> names;

    if (!reader.readNextStartElement() || reader.name() != QLatin1String("FrameForge")) {
        if (!reader.hasError())
            reader.raiseError("This is not a FrameForge file.");
    } else if (reader.attributes().value("version") != FormatVersion) {
        reader.raiseError(QString("Unsupported file version '%1'.")
                              .arg(reader.attributes().value("version").toString()));
    }

    while (!reader.hasError() && reader.readNextStartElement()) {
        if (reader.name() != QLatin1String("Signals")) {
            reader.skipCurrentElement();
            continue;
        }
        while (!reader.hasError() && reader.readNextStartElement()) {
            if (reader.name() != QLatin1String("Signal")) {
                reader.skipCurrentElement();
                continue;
            }
            const Signal signal = readSignal(reader);
            if (!reader.hasError() && names.contains(signal.name))
                reader.raiseError(QString("There are two signals named '%1'.").arg(signal.name));
            names.insert(signal.name);
            loadedSignals.append(signal);
        }
    }

    if (reader.hasError()) {
        *errorMessage = QString("Line %1: %2").arg(reader.lineNumber()).arg(reader.errorString());
        return false;
    }

    database.replaceAllSignals(loadedSignals);
    return true;
}

} // namespace XmlStorage
