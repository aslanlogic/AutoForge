#ifndef MESSAGE_H
#define MESSAGE_H

#include "network.h"
#include "signal.h"

#include <QHash>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace ff {

// Places one signal inside a message. The same signal may be placed in
// several messages; the Signal itself does not know where it is used.
struct SignalMapping
{
    int signalId = 0; // Signal::id
    int startBit = 0; // see signalBits() in signal.h for what this means
};

// Definition of one message (frame) on a network: how it is identified on the
// bus, how long its payload is and how often it is sent. A message has no
// protocol of its own: it uses the network's (see Network::protocol), which
// decides the valid range of frameId and length (see the functions below).
struct Message
{
    // CAN and CAN FD only: an 11-bit (Standard) or 29-bit (Extended) ID.
    enum class IdFormat { Standard, Extended };
    enum class TxMethod { Cyclic, OnEvent, CyclicAndOnEvent };

    int id = 0; // unique, assigned by Database when added; 0 = not added yet
    QString name;
    IdFormat idFormat = IdFormat::Standard; // ignored on UART and UDP networks
    quint32 frameId = 0; // the identifier on the bus, e.g. the CAN ID
    int length = 8;      // payload length in bytes (the DLC for CAN)
    TxMethod txMethod = TxMethod::Cyclic;
    int cycleTime = 100; // in ms; only used when txMethod sends cyclically
    QVector<SignalMapping> signalMappings; // the signals this message carries
    int transmitterId = 0;   // Node::id of the one node that sends it; 0 = none
    QVector<int> receiverIds; // Node::id of every node that receives it
    QString comment;
};

// Equal if every field is equal. Used to tell whether an edit changed anything.
inline bool operator==(const SignalMapping &a, const SignalMapping &b)
{
    return a.signalId == b.signalId && a.startBit == b.startBit;
}

inline bool operator==(const Message &a, const Message &b)
{
    return a.id == b.id && a.name == b.name && a.idFormat == b.idFormat && a.frameId == b.frameId
        && a.length == b.length && a.txMethod == b.txMethod && a.cycleTime == b.cycleTime
        && a.signalMappings == b.signalMappings && a.transmitterId == b.transmitterId
        && a.receiverIds == b.receiverIds && a.comment == b.comment;
}

// Display names, in the same wording as the Message dialog's combo boxes.
inline QString toString(Message::IdFormat idFormat)
{
    switch (idFormat) {
    case Message::IdFormat::Standard: return "Standard (11 bit)";
    case Message::IdFormat::Extended: return "Extended (29 bit)";
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

// CAN and CAN FD: messages have an ID format and a DLC.
inline bool isCan(Network::Protocol protocol)
{
    return protocol == Network::Protocol::Can || protocol == Network::Protocol::CanFd;
}

inline bool isCyclic(Message::TxMethod txMethod)
{
    return txMethod != Message::TxMethod::OnEvent;
}

// Largest allowed frameId on this network: 11 or 29 bits for CAN and CAN FD;
// for UART and UDP whatever fits in the network's ID size (1 byte: 0xFF, ...).
inline quint32 maxFrameId(const Network &network, Message::IdFormat idFormat)
{
    auto largestFor = [](int bytes) { return bytes >= 4 ? 0xFFFFFFFFu : (1u << (8 * bytes)) - 1; };
    switch (network.protocol) {
    case Network::Protocol::Can:
    case Network::Protocol::CanFd: return idFormat == Message::IdFormat::Extended ? 0x1FFFFFFF : 0x7FF;
    case Network::Protocol::Uart:  return largestFor(network.uart.idSize);
    case Network::Protocol::Udp:   return largestFor(network.udp.idSize);
    }
    return 0;
}

// Largest payload in bytes. UART: the DLC is one byte. UDP: the most one datagram can carry.
inline int maxLength(Network::Protocol protocol)
{
    switch (protocol) {
    case Network::Protocol::Can:   return 8;
    case Network::Protocol::CanFd: return 64;
    case Network::Protocol::Uart:  return 255;
    case Network::Protocol::Udp:   return 65507;
    }
    return 0;
}

// CAN FD only allows 0-8, 12, 16, 20, 24, 32, 48 and 64 bytes.
inline bool isValidLength(Network::Protocol protocol, int length)
{
    if (length < 0 || length > maxLength(protocol))
        return false;
    if (protocol == Network::Protocol::CanFd) {
        return length <= 8 || length == 12 || length == 16 || length == 20
            || length == 24 || length == 32 || length == 48 || length == 64;
    }
    return true;
}

// Checks a message's ID and length against the network. Returns an empty string
// if they fit, otherwise what is wrong.
inline QString checkMessageAgainstNetwork(const Message &message, const Network &network)
{
    const quint32 largestId = maxFrameId(network, message.idFormat);
    if (message.frameId > largestId)
        return QString("Message '%1': ID %2 is too big for %3 (at most %4).")
            .arg(message.name, frameIdToText(message.frameId), toString(network.protocol),
                 frameIdToText(largestId));
    if (!isValidLength(network.protocol, message.length))
        return QString("Message '%1': a length of %2 bytes is not possible on %3.")
            .arg(message.name).arg(message.length).arg(toString(network.protocol));
    return QString();
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

} // namespace ff

#endif // MESSAGE_H
