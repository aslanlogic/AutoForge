#ifndef BITCODEC_H
#define BITCODEC_H

#include <QByteArray>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <cstring>

// Reads and writes one value inside a payload. Both cores number the bits the same
// way (see ff::signalBits() and usc::dataTypeBits()): byte * 8 + bit, and the list
// is least significant bit first for Intel, most significant bit first for Motorola.
namespace BitCodec {

enum class Kind { Signed, Unsigned, Float, Double };

inline quint64 mask(int length)
{
    return length >= 64 ? ~quint64(0) : (quint64(1) << length) - 1;
}

inline quint64 extract(const QByteArray &data, const QVector<int> &bits, bool lsbFirst)
{
    quint64 raw = 0;
    const int count = bits.size();
    for (int i = 0; i < count; ++i) {
        const int byte = bits[i] / 8;
        if (byte < 0 || byte >= data.size())
            continue;
        if ((quint8(data[byte]) >> (bits[i] % 8)) & 1)
            raw |= quint64(1) << (lsbFirst ? i : count - 1 - i);
    }
    return raw;
}

inline void insert(QByteArray &data, const QVector<int> &bits, bool lsbFirst, quint64 raw)
{
    const int count = bits.size();
    for (int i = 0; i < count; ++i) {
        const int byte = bits[i] / 8;
        if (byte < 0 || byte >= data.size())
            continue;
        const bool set = (raw >> (lsbFirst ? i : count - 1 - i)) & 1;
        const quint8 bit = quint8(1u << (bits[i] % 8));
        data[byte] = char(set ? (quint8(data[byte]) | bit) : (quint8(data[byte]) & ~bit));
    }
}

// The raw value as an integer: sign-extended for Signed. Value tables use this.
inline qint64 rawInteger(quint64 raw, int length, Kind kind)
{
    if (kind == Kind::Signed && length < 64 && ((raw >> (length - 1)) & 1))
        return qint64(raw | ~mask(length));
    return qint64(raw);
}

// physical = raw * factor + offset
inline double toPhysical(quint64 raw, int length, Kind kind, double factor, double offset)
{
    double value = 0.0;
    switch (kind) {
    case Kind::Signed:   value = double(rawInteger(raw, length, kind)); break;
    case Kind::Unsigned: value = double(raw); break;
    case Kind::Float: {
        const quint32 bits = quint32(raw);
        float f;
        std::memcpy(&f, &bits, sizeof f);
        value = f;
        break;
    }
    case Kind::Double:
        std::memcpy(&value, &raw, sizeof value);
        break;
    }
    return value * factor + offset;
}

// The raw value for a physical value, rounded and clamped to what fits in `length` bits.
inline quint64 fromPhysical(double physical, int length, Kind kind, double factor, double offset)
{
    const double scaled = factor != 0.0 ? (physical - offset) / factor : 0.0;
    switch (kind) {
    case Kind::Signed: {
        const double largest = std::ldexp(1.0, length - 1) - 1;
        const double rounded = std::round(qBound(-largest - 1, scaled, largest));
        return quint64(qint64(rounded)) & mask(length);
    }
    case Kind::Unsigned: {
        const double largest = std::ldexp(1.0, length) - 1;
        const double rounded = std::round(qBound(0.0, scaled, largest));
        return rounded >= 18446744073709549568.0 ? ~quint64(0) : quint64(rounded);
    }
    case Kind::Float: {
        const float f = float(scaled);
        quint32 bits;
        std::memcpy(&bits, &f, sizeof bits);
        return bits;
    }
    case Kind::Double: {
        quint64 bits;
        std::memcpy(&bits, &scaled, sizeof bits);
        return bits;
    }
    }
    return 0;
}

} // namespace BitCodec

#endif // BITCODEC_H
