#include "xmlstorage.h"
#include "database.h"

#include <QFile>
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
        if (reader.name() == QLatin1String("SecurityLevels")) {
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

    database.replaceAll(Network(), loadedSecurityLevels);
    return true;
}

} // namespace XmlStorage

} // namespace usc
