#ifndef SIGNAL_H
#define SIGNAL_H

#include <QString>
#include <QVector>

// Definition of one signal: a bit range inside a frame's payload and how to
// convert its raw value into a physical value (physical = raw * factor + offset).
struct Signal
{
    enum class ByteOrder { Intel, Motorola };
    enum class ValueType { Signed, Unsigned, IeeeFloat, IeeeDouble };

    int id = 0; // unique, assigned by Database when added; 0 = not added yet
    QString name;
    int length = 8; // in bits
    ByteOrder byteOrder = ByteOrder::Intel;
    ValueType valueType = ValueType::Signed;
    QString unit;
    double initialValue = 0.0;
    double factor = 1.0;
    double offset = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    QString comment;
};

// Display names, in the same wording as the Signal dialog's combo boxes.
inline QString toString(Signal::ByteOrder byteOrder)
{
    switch (byteOrder) {
    case Signal::ByteOrder::Intel:    return "Intel";
    case Signal::ByteOrder::Motorola: return "Motorola";
    }
    return QString();
}

// The bits a signal covers inside a message payload, numbered byte * 8 + bit,
// where bit 0 is the least significant bit of a byte (so bit 8 is byte 1, bit 0).
//
// startBit follows the DBC convention:
//  - Intel (little endian):   startBit is the signal's least significant bit and
//                             the signal continues upwards: 0, 1, 2, ... 7, 8, 9, ...
//  - Motorola (big endian):   startBit is the signal's most significant bit and the
//                             signal continues downwards within a byte, then into the
//                             next byte from its top bit: 3, 2, 1, 0, 15, 14, ...
// Intel bits are returned least significant first, Motorola bits most significant first.
inline QVector<int> signalBits(int startBit, int length, Signal::ByteOrder byteOrder)
{
    QVector<int> bits;
    int bit = startBit;
    for (int i = 0; i < length; ++i) {
        bits.append(bit);
        if (byteOrder == Signal::ByteOrder::Intel)
            bit += 1;
        else
            bit = (bit % 8 == 0) ? bit + 15 : bit - 1;
    }
    return bits;
}

inline QString toString(Signal::ValueType valueType)
{
    switch (valueType) {
    case Signal::ValueType::Signed:     return "Signed";
    case Signal::ValueType::Unsigned:   return "Unsigned";
    case Signal::ValueType::IeeeFloat:  return "IEEE Float";
    case Signal::ValueType::IeeeDouble: return "IEEE Double";
    }
    return QString();
}

#endif // SIGNAL_H
