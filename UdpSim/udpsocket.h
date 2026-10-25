#ifndef UDPSIM_UDPSOCKET_H
#define UDPSIM_UDPSOCKET_H

#include "bytes.h"

#include <cstdint>
#include <string>

// A UDP "bus" in the format DiagProbe, FrameForge and UDScribe use: every
// datagram is one message,
//     [message ID, 4 bytes, little-endian (lowest byte first)] [payload]
// This is the only file that uses the operating system's socket API (POSIX);
// standard C++ has no networking.
class UdpSocket
{
public:
    UdpSocket() = default;
    ~UdpSocket();
    UdpSocket(const UdpSocket &) = delete;
    UdpSocket &operator=(const UdpSocket &) = delete;

    // Listens on every network interface at port and sends to remoteAddress:remotePort.
    // Returns false and says why if that is not possible.
    bool open(std::uint16_t port, const std::string &remoteAddress, std::uint16_t remotePort,
              std::string *errorMessage);

    void send(std::uint32_t id, const Bytes &payload);

    // Waits up to timeoutMs for a message. True if one came; false on timeout,
    // on a datagram that is not a message, or when a signal (Ctrl+C) interrupts.
    bool receive(int timeoutMs, std::uint32_t *id, Bytes *payload);

private:
    int m_socket = -1;
    std::string m_remoteAddress;
    std::uint16_t m_remotePort = 0;
};

#endif // UDPSIM_UDPSOCKET_H
