#ifndef DATAIDENTIFIER_H
#define DATAIDENTIFIER_H

#include "datatype.h"

#include <QHash>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace usc {

// Places one data type inside a DID's data record. The same data type may be
// placed several times, so every placement has its own name, unique in the DID.
struct DataTypeMapping
{
    QString name;     // e.g. "FrontLeft"; defaults to the data type's name
    int dataTypeId = 0; // DataType::id
    int startBit = 0;   // see dataTypeBits() in datatype.h for what this means
};

// One data identifier (DID): a 2-byte number that names a fixed-length data
// record, made of data types placed at bit positions. ReadDataByIdentifier (0x22)
// reads it; WriteDataByIdentifier (0x2E) will write it.
struct DataIdentifier
{
    int id = 0; // unique, assigned by Database when added; 0 = not added yet
    QString name;
    quint16 did = 0xF190;
    int length = 1; // data record length in bytes, 1..MaxDataRecordLength
    QVector<DataTypeMapping> mappings; // the data types it carries; must fit and not overlap
    // Session::id of every session the DID can be read in. In any other session
    // the ECU answers NRC 0x31 (requestOutOfRange).
    QVector<int> readSessionIds;
    // SecurityLevel::id of the levels that allow reading: any of them unlocked
    // (otherwise NRC 0x33). Empty = no security needed.
    QVector<int> readSecurityLevelIds;
    QString comment;
};

// Equal if every field is equal. Used to tell whether an edit changed anything.
inline bool operator==(const DataTypeMapping &a, const DataTypeMapping &b)
{
    return a.name == b.name && a.dataTypeId == b.dataTypeId && a.startBit == b.startBit;
}

inline bool operator==(const DataIdentifier &a, const DataIdentifier &b)
{
    return a.id == b.id && a.name == b.name && a.did == b.did && a.length == b.length
        && a.mappings == b.mappings && a.readSessionIds == b.readSessionIds
        && a.readSecurityLevelIds == b.readSecurityLevelIds && a.comment == b.comment;
}

// DIDs are shown and saved as four-digit hex: "0xF190".
inline QString didToText(quint16 did)
{
    return "0x" + QString("%1").arg(uint(did), 4, 16, QChar('0')).toUpper();
}

// Checks that every placed data type lies inside the data record and that no two
// share a bit. Returns an empty string if all is fine, otherwise what is wrong.
inline QString checkDataTypeLayout(int lengthInBytes, const QVector<DataTypeMapping> &mappings,
                                   const QHash<int, DataType> &dataTypesById)
{
    QHash<int, QString> owner; // bit -> name of the placement using it
    for (const DataTypeMapping &mapping : mappings) {
        const DataType dataType = dataTypesById.value(mapping.dataTypeId);
        for (int bit : dataTypeBits(mapping.startBit, dataType.length, dataType.byteOrder)) {
            if (bit < 0 || bit >= lengthInBytes * 8)
                return QString("'%1' does not fit in %2 bytes.").arg(mapping.name).arg(lengthInBytes);
            if (owner.contains(bit))
                return QString("'%1' and '%2' overlap.").arg(owner.value(bit), mapping.name);
            owner.insert(bit, mapping.name);
        }
    }
    return QString();
}

} // namespace usc

#endif // DATAIDENTIFIER_H
