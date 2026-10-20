#ifndef MESSAGE_H
#define MESSAGE_H

#include "signal.h"

#include <QHash>
#include <QString>
#include <QVector>
#include <QtGlobal>

// Places one signal inside a message. The same signal may be placed in
// several messages; the Signal itself does not know where it is used.
struct SignalMapping
{
    int signalId = 0; // Signal::id
    int startBit = 0; // see signalBits() in signal.h for what this means
};

// Definition of one message (frame) on a network: how it is identified on the
// bus, how long its payload is and how often it is sent.
struct Message
{
    // The protocol decides the valid range of frameId and length (see below).
    enum class Type { CanStandard, CanExtended, CanFdStandard, CanFdExtended, Uart, Udp };
    enum class TxMethod { Cyclic, OnEvent, CyclicAndOnEvent };

    int id = 0; // unique, assigned by Database when added; 0 = not added yet
    QString name;
    Type type = Type::CanStandard;
    quint32 frameId = 0; // the identifier on the bus, e.g. the CAN ID
    int length = 8;      // payload length in bytes (the DLC for CAN)
    TxMethod txMethod = TxMethod::Cyclic;
    int cycleTime = 100; // in ms; only used when txMethod sends cyclically
    QVector<SignalMapping> signalMappings; // the signals this message carries
    QString comment;
};

// Display names, in the same wording as the Message dialog's combo boxes.
inline QString toString(Message::Type type)
{
    switch (type) {
    case Message::Type::CanStandard:   return "CAN Standard";
    case Message::Type::CanExtended:   return "CAN Extended";
    case Message::Type::CanFdStandard: return "CAN FD Standard";
    case Message::Type::CanFdExtended: return "CAN FD Extended";
    case Message::Type::Uart:          return "UART";
    case Message::Type::Udp:           return "UDP";
    }
    return QString();
}

inline QString toString(Message::TxMethod txMethod)
{
    switch (txMethod) {
    case Message::TxMethod::Cyclic:           return "Cyclic";
    case Message::TxMethod::OnEvent:          return "On Event";
    case Message::TxMethod::CyclicAndOnEvent: return "Cyclic and On Event";
    }
    return QString();
}

// Frame IDs are shown and saved as hex: "0x1A".
inline QString frameIdToText(quint32 frameId)
{
    return "0x" + QString::number(frameId, 16).toUpper();
}

inline bool isCan(Message::Type type)
{
    return type == Message::Type::CanStandard || type == Message::Type::CanExtended
        || type == Message::Type::CanFdStandard || type == Message::Type::CanFdExtended;
}

inline bool isCyclic(Message::TxMethod txMethod)
{
    return txMethod != Message::TxMethod::OnEvent;
}

// Largest allowed frameId: 11-bit or 29-bit for CAN, any 32-bit number otherwise.
inline quint32 maxFrameId(Message::Type type)
{
    switch (type) {
    case Message::Type::CanStandard:
    case Message::Type::CanFdStandard: return 0x7FF;
    case Message::Type::CanExtended:
    case Message::Type::CanFdExtended: return 0x1FFFFFFF;
    case Message::Type::Uart:
    case Message::Type::Udp:           return 0xFFFFFFFF;
    }
    return 0;
}

// Largest payload in bytes. UDP: the most one datagram can carry.
inline int maxLength(Message::Type type)
{
    switch (type) {
    case Message::Type::CanStandard:
    case Message::Type::CanExtended:   return 8;
    case Message::Type::CanFdStandard:
    case Message::Type::CanFdExtended: return 64;
    case Message::Type::Uart:          return 65535;
    case Message::Type::Udp:           return 65507;
    }
    return 0;
}

// CAN FD only allows 0-8, 12, 16, 20, 24, 32, 48 and 64 bytes.
inline bool isValidLength(Message::Type type, int length)
{
    if (length < 0 || length > maxLength(type))
        return false;
    if (type == Message::Type::CanFdStandard || type == Message::Type::CanFdExtended) {
        return length <= 8 || length == 12 || length == 16 || length == 20
            || length == 24 || length == 32 || length == 48 || length == 64;
    }
    return true;
}

// Checks that every placed signal lies inside the payload and that no two signals
// share a bit. Returns an empty string if all is fine, otherwise what is wrong.
inline QString checkSignalLayout(int lengthInBytes, const QVector<SignalMapping> &mappings,
                                 const QHash<int, Signal> &signalsById)
{
    QHash<int, QString> owner; // bit -> name of the signal using it
    for (const SignalMapping &mapping : mappings) {
        const Signal signal = signalsById.value(mapping.signalId);
        for (int bit : signalBits(mapping.startBit, signal.length, signal.byteOrder)) {
            if (bit < 0 || bit >= lengthInBytes * 8)
                return QString("Signal '%1' does not fit in %2 bytes.").arg(signal.name).arg(lengthInBytes);
            if (owner.contains(bit))
                return QString("Signals '%1' and '%2' overlap.").arg(owner.value(bit), signal.name);
            owner.insert(bit, signal.name);
        }
    }
    return QString();
}

#endif // MESSAGE_H
