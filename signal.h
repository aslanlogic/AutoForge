#ifndef SIGNAL_H
#define SIGNAL_H

#include <QString>

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

#endif // SIGNAL_H
