#ifndef USCCORE_DATATYPE_H
#define USCCORE_DATATYPE_H

#include <QString>
#include <QVector>

namespace usc {

// Definition of one data type: a bit range inside a DID's data record and how to
// read its raw value. Numbers convert to a physical value (physical = raw * factor
// + offset); Ascii and Bytes are shown as text and hex. A data type is shared: any
// number of DIDs can place it, even several times.
struct DataType
{
    enum class ByteOrder { Intel, Motorola };
    enum class ValueType { Signed, Unsigned, IeeeFloat, IeeeDouble, Ascii, Bytes };

    int id = 0; // unique, assigned by Database when added; 0 = not added yet
    QString name;
    int length = 8; // in bits; whole bytes for Ascii and Bytes (see needsWholeBytes())
    ByteOrder byteOrder = ByteOrder::Intel;
    ValueType valueType = ValueType::Unsigned;
    // Numbers only; ignored for Ascii and Bytes.
    QString unit;
    double factor = 1.0;
    double offset = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    int valueTableId = 0; // ValueTable::id that names this data type's raw values; 0 = none
    QString comment;
};

// Equal if every field is equal. Used to tell whether an edit changed anything.
inline bool operator==(const DataType &a, const DataType &b)
{
    return a.id == b.id && a.name == b.name && a.length == b.length && a.byteOrder == b.byteOrder
        && a.valueType == b.valueType && a.unit == b.unit && a.factor == b.factor && a.offset == b.offset
        && a.minimum == b.minimum && a.maximum == b.maximum && a.valueTableId == b.valueTableId
        && a.comment == b.comment;
}

// Text and raw bytes are made of whole bytes, so their length is a multiple of 8 bits.
inline bool needsWholeBytes(DataType::ValueType valueType)
{
    return valueType == DataType::ValueType::Ascii || valueType == DataType::ValueType::Bytes;
}

// The largest data record: the positive response 0x62 + DID (2 bytes) + data must fit
// in one ISO-TP message of 4095 bytes.
const int MaxDataRecordLength = 4092; // bytes

// Checks a data type's length against its value type: 1-64 bits for integers, 32 for
// IEEE Float, 64 for IEEE Double, whole bytes up to MaxDataRecordLength for Ascii and
// Bytes. Returns an empty string if it fits, otherwise what is wrong.
inline QString checkLength(const DataType &dataType)
{
    switch (dataType.valueType) {
    case DataType::ValueType::Signed:
    case DataType::ValueType::Unsigned:
        if (dataType.length < 1 || dataType.length > 64)
            return "The length of an integer must be from 1 to 64 bits.";
        break;
    case DataType::ValueType::IeeeFloat:
        if (dataType.length != 32)
            return "An IEEE Float is 32 bits long.";
        break;
    case DataType::ValueType::IeeeDouble:
        if (dataType.length != 64)
            return "An IEEE Double is 64 bits long.";
        break;
    case DataType::ValueType::Ascii:
    case DataType::ValueType::Bytes:
        if (dataType.length < 8 || dataType.length % 8 != 0 || dataType.length > MaxDataRecordLength * 8)
            return QString("The length of ASCII and Bytes must be whole bytes, from 8 to %1 bits.")
                .arg(MaxDataRecordLength * 8);
        break;
    }
    return QString();
}

// Display names, in the same wording as the Data Type dialog's combo boxes.
inline QString toString(DataType::ByteOrder byteOrder)
{
    switch (byteOrder) {
    case DataType::ByteOrder::Intel:    return "Intel";
    case DataType::ByteOrder::Motorola: return "Motorola";
    }
    return QString();
}

// The bits a data type covers inside a data record, numbered byte * 8 + bit,
// where bit 0 is the least significant bit of a byte (so bit 8 is byte 1, bit 0).
//
// startBit follows the DBC convention:
//  - Intel (little endian):   startBit is the data type's least significant bit and
//                             it continues upwards: 0, 1, 2, ... 7, 8, 9, ...
//  - Motorola (big endian):   startBit is the data type's most significant bit and it
//                             continues downwards within a byte, then into the next
//                             byte from its top bit: 3, 2, 1, 0, 15, 14, ...
// Intel bits are returned least significant first, Motorola bits most significant first.
inline QVector<int> dataTypeBits(int startBit, int length, DataType::ByteOrder byteOrder)
{
    QVector<int> bits;
    int bit = startBit;
    for (int i = 0; i < length; ++i) {
        bits.append(bit);
        if (byteOrder == DataType::ByteOrder::Intel)
            bit += 1;
        else
            bit = (bit % 8 == 0) ? bit + 15 : bit - 1;
    }
    return bits;
}

inline QString toString(DataType::ValueType valueType)
{
    switch (valueType) {
    case DataType::ValueType::Signed:     return "Signed";
    case DataType::ValueType::Unsigned:   return "Unsigned";
    case DataType::ValueType::IeeeFloat:  return "IEEE Float";
    case DataType::ValueType::IeeeDouble: return "IEEE Double";
    case DataType::ValueType::Ascii:      return "ASCII";
    case DataType::ValueType::Bytes:      return "Bytes";
    }
    return QString();
}

} // namespace usc

#endif // USCCORE_DATATYPE_H
