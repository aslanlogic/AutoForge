#ifndef NETWORK_H
#define NETWORK_H

#include <QString>

namespace ff {

// The settings each protocol needs. Only the one of the network's active
// protocol is used (see Network below).

struct CanSettings
{
    int bitrate = 500000;      // bit/s
    double samplePoint = 87.5; // % of the bit time
};

struct CanFdSettings
{
    // Arbitration phase (ID and control bits) and data phase (payload) run at different speeds.
    int nominalBitrate = 500000; // bit/s
    double nominalSamplePoint = 80.0;
    int dataBitrate = 2000000;   // bit/s
    double dataSamplePoint = 80.0;
    bool iso = true;             // ISO CAN FD; false = the older non-ISO (Bosch) variant
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

// The network of a document. It runs exactly one protocol at a time; only the
// settings of that protocol are used and saved, the others keep their defaults.
struct Network
{
    enum class Protocol { Can, CanFd, Uart, Udp };

    Protocol protocol = Protocol::Can;
    CanSettings can;
    CanFdSettings canFd;
    UartSettings uart;
    UdpSettings udp;
};

// Equal if every setting is equal. Used to tell whether an edit changed anything.
inline bool operator==(const CanSettings &a, const CanSettings &b)
{
    return a.bitrate == b.bitrate && a.samplePoint == b.samplePoint;
}

inline bool operator==(const CanFdSettings &a, const CanFdSettings &b)
{
    return a.nominalBitrate == b.nominalBitrate && a.nominalSamplePoint == b.nominalSamplePoint
        && a.dataBitrate == b.dataBitrate && a.dataSamplePoint == b.dataSamplePoint && a.iso == b.iso;
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
    return a.protocol == b.protocol && a.can == b.can && a.canFd == b.canFd && a.uart == b.uart && a.udp == b.udp;
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

} // namespace ff

#endif // NETWORK_H
