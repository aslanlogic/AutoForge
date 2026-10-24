#ifndef NETWORK_H
#define NETWORK_H

#include <QString>
#include <QtGlobal>

namespace usc {

// The settings each protocol needs. Only the one of the network's active
// protocol is used (see Network below).

// CAN and CAN FD: an 11-bit (Standard) or 29-bit (Extended) ID.
enum class IdFormat { Standard, Extended };

struct CanSettings
{
    int bitrate = 500000;      // bit/s
    double samplePoint = 87.5; // % of the bit time
    IdFormat idFormat = IdFormat::Standard; // of the request/response IDs
};

struct CanFdSettings
{
    // Arbitration phase (ID and control bits) and data phase (payload) run at different speeds.
    int nominalBitrate = 500000; // bit/s
    double nominalSamplePoint = 80.0;
    int dataBitrate = 2000000;   // bit/s
    double dataSamplePoint = 80.0;
    bool iso = true;             // ISO CAN FD; false = the older non-ISO (Bosch) variant
    IdFormat idFormat = IdFormat::Standard; // of the request/response IDs
};

struct UartSettings
{
    enum class Parity { None, Even, Odd, Mark, Space };
    enum class StopBits { One, OneAndHalf, Two };
    enum class FlowControl { None, Hardware, Software }; // Hardware = RTS/CTS, Software = XON/XOFF
    // The exact CRC variant of each size is fixed, so both ends of the line
    // always compute the same value. "Check" is the CRC of the ASCII text
    // "123456789"; use it to test an implementation.
    //
    //   Crc8   CRC-8/SMBUS          poly 0x07        init 0x00        not reflected  xorout 0x00        check 0xF4
    //   Crc16  CRC-16/CCITT-FALSE   poly 0x1021      init 0xFFFF      not reflected  xorout 0x0000      check 0x29B1
    //          (also called CRC-16/IBM-3740)
    //   Crc32  CRC-32/ISO-HDLC      poly 0x04C11DB7  init 0xFFFFFFFF  reflected      xorout 0xFFFFFFFF  check 0xCBF43926
    //          (the one used by zlib and Ethernet)
    enum class Crc { Crc8, Crc16, Crc32 };

    int baudRate = 115200;
    int dataBits = 8;
    Parity parity = Parity::None;
    StopBits stopBits = StopBits::One;
    FlowControl flowControl = FlowControl::None;

    // How messages are framed on the serial line: each message is
    //   [message ID, idSize bytes] [DLC, 1 byte] [payload, DLC bytes] [CRC, 1/2/4 bytes]
    // COBS-encoded and ended with a 0x00 byte. COBS removes every 0x00 from the
    // encoded bytes, so a 0x00 always marks the end of a message.
    //  - The CRC is computed over [ID] [DLC] [payload], before COBS encoding.
    //  - The ID and the CRC are sent little-endian (lowest byte first).
    int idSize = 1; // bytes
    Crc crc = Crc::Crc16;
};

struct UdpSettings
{
    enum class Mode { Unicast, Broadcast, Multicast };

    Mode mode = Mode::Unicast;
    QString localAddress = "0.0.0.0"; // 0.0.0.0 = every network interface
    int localPort = 5000;
    QString remoteAddress = "192.168.0.10"; // unicast only
    int remotePort = 5000;                  // also the port of the broadcast / multicast group
    QString multicastGroup = "239.0.0.1";   // multicast only

    // Every datagram starts with the message ID (idSize bytes), followed by the payload.
    int idSize = 4; // bytes
};

// The network the ECU is reached on. It runs exactly one protocol at a time; only
// the settings of that protocol are used and saved, the others keep their defaults.
struct Network
{
    enum class Protocol { Can, CanFd, Uart, Udp };

    Protocol protocol = Protocol::Can;
    CanSettings can;
    CanFdSettings canFd;
    UartSettings uart;
    UdpSettings udp;

    // How diagnostic requests and responses are identified on the bus: the CAN ID
    // on CAN and CAN FD (see their idFormat), the message ID of the framing on UART and UDP.
    quint32 requestId = 0x7E0;  // tester -> ECU
    quint32 responseId = 0x7E8; // ECU -> tester
};

// Equal if every setting is equal. Used to tell whether an edit changed anything.
inline bool operator==(const CanSettings &a, const CanSettings &b)
{
    return a.bitrate == b.bitrate && a.samplePoint == b.samplePoint && a.idFormat == b.idFormat;
}

inline bool operator==(const CanFdSettings &a, const CanFdSettings &b)
{
    return a.nominalBitrate == b.nominalBitrate && a.nominalSamplePoint == b.nominalSamplePoint
        && a.dataBitrate == b.dataBitrate && a.dataSamplePoint == b.dataSamplePoint && a.iso == b.iso
        && a.idFormat == b.idFormat;
}

inline bool operator==(const UartSettings &a, const UartSettings &b)
{
    return a.baudRate == b.baudRate && a.dataBits == b.dataBits && a.parity == b.parity
        && a.stopBits == b.stopBits && a.flowControl == b.flowControl && a.idSize == b.idSize && a.crc == b.crc;
}

inline bool operator==(const UdpSettings &a, const UdpSettings &b)
{
    return a.mode == b.mode && a.localAddress == b.localAddress && a.localPort == b.localPort
        && a.remoteAddress == b.remoteAddress && a.remotePort == b.remotePort
        && a.multicastGroup == b.multicastGroup && a.idSize == b.idSize;
}

inline bool operator==(const Network &a, const Network &b)
{
    return a.protocol == b.protocol && a.can == b.can && a.canFd == b.canFd && a.uart == b.uart && a.udp == b.udp
        && a.requestId == b.requestId && a.responseId == b.responseId;
}

// Display names, in the same wording as the Network dialog's combo boxes.
inline QString toString(Network::Protocol protocol)
{
    switch (protocol) {
    case Network::Protocol::Can:   return "CAN";
    case Network::Protocol::CanFd: return "CAN FD";
    case Network::Protocol::Uart:  return "UART";
    case Network::Protocol::Udp:   return "UDP";
    }
    return QString();
}

inline QString toString(IdFormat idFormat)
{
    return idFormat == IdFormat::Extended ? "Extended (29 bit)" : "Standard (11 bit)";
}

inline QString toString(UartSettings::Parity parity)
{
    switch (parity) {
    case UartSettings::Parity::None:  return "None";
    case UartSettings::Parity::Even:  return "Even";
    case UartSettings::Parity::Odd:   return "Odd";
    case UartSettings::Parity::Mark:  return "Mark";
    case UartSettings::Parity::Space: return "Space";
    }
    return QString();
}

inline QString toString(UartSettings::StopBits stopBits)
{
    switch (stopBits) {
    case UartSettings::StopBits::One:        return "1";
    case UartSettings::StopBits::OneAndHalf: return "1.5";
    case UartSettings::StopBits::Two:        return "2";
    }
    return QString();
}

inline QString toString(UartSettings::FlowControl flowControl)
{
    switch (flowControl) {
    case UartSettings::FlowControl::None:     return "None";
    case UartSettings::FlowControl::Hardware: return "Hardware (RTS/CTS)";
    case UartSettings::FlowControl::Software: return "Software (XON/XOFF)";
    }
    return QString();
}

inline QString toString(UartSettings::Crc crc)
{
    switch (crc) {
    case UartSettings::Crc::Crc8:  return "CRC-8 (SMBUS)";
    case UartSettings::Crc::Crc16: return "CRC-16 (CCITT-FALSE)";
    case UartSettings::Crc::Crc32: return "CRC-32 (ISO-HDLC, as zlib)";
    }
    return QString();
}

inline QString toString(UdpSettings::Mode mode)
{
    switch (mode) {
    case UdpSettings::Mode::Unicast:   return "Unicast";
    case UdpSettings::Mode::Broadcast: return "Broadcast";
    case UdpSettings::Mode::Multicast: return "Multicast";
    }
    return QString();
}

// IDs are shown and saved as hex: "0x7E0".
inline QString idToText(quint32 id)
{
    return "0x" + QString::number(id, 16).toUpper();
}

// CAN and CAN FD: IDs have an ID format.
inline bool isCan(Network::Protocol protocol)
{
    return protocol == Network::Protocol::Can || protocol == Network::Protocol::CanFd;
}

// Largest allowed request/response ID on this network: 11 or 29 bits for CAN and
// CAN FD (by their idFormat); for UART and UDP whatever fits in the network's ID
// size (1 byte: 0xFF, ...).
inline quint32 maxId(const Network &network)
{
    auto largestFor = [](int bytes) { return bytes >= 4 ? 0xFFFFFFFFu : (1u << (8 * bytes)) - 1; };
    auto largestCan = [](IdFormat idFormat) { return idFormat == IdFormat::Extended ? 0x1FFFFFFFu : 0x7FFu; };
    switch (network.protocol) {
    case Network::Protocol::Can:   return largestCan(network.can.idFormat);
    case Network::Protocol::CanFd: return largestCan(network.canFd.idFormat);
    case Network::Protocol::Uart:  return largestFor(network.uart.idSize);
    case Network::Protocol::Udp:   return largestFor(network.udp.idSize);
    }
    return 0;
}

} // namespace usc

#endif // NETWORK_H
