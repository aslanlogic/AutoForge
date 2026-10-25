#include "udpsocket.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace {

const std::size_t IdSize = 4; // bytes; idSize="4" in both example files

} // namespace

UdpSocket::~UdpSocket()
{
    if (m_socket >= 0)
        ::close(m_socket);
}

bool UdpSocket::open(std::uint16_t port, const std::string &remoteAddress, std::uint16_t remotePort,
                     std::string *errorMessage)
{
    m_socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        *errorMessage = std::string("Could not create a UDP socket: ") + std::strerror(errno);
        return false;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (::bind(m_socket, reinterpret_cast<sockaddr *>(&address), sizeof address) < 0) {
        *errorMessage = "Could not listen on UDP port " + std::to_string(port) + ": " + std::strerror(errno);
        return false;
    }
    in_addr remote{};
    if (::inet_pton(AF_INET, remoteAddress.c_str(), &remote) != 1) {
        *errorMessage = "Not an IPv4 address: " + remoteAddress;
        return false;
    }
    m_remoteAddress = remoteAddress;
    m_remotePort = remotePort;
    return true;
}

void UdpSocket::send(std::uint32_t id, const Bytes &payload)
{
    Bytes datagram;
    for (std::size_t i = 0; i < IdSize; ++i)
        datagram.push_back(std::uint8_t(id >> (8 * i)));
    datagram.insert(datagram.end(), payload.begin(), payload.end());

    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(m_remotePort);
    ::inet_pton(AF_INET, m_remoteAddress.c_str(), &remote.sin_addr);
    ::sendto(m_socket, datagram.data(), datagram.size(), 0, reinterpret_cast<sockaddr *>(&remote), sizeof remote);
}

bool UdpSocket::receive(int timeoutMs, std::uint32_t *id, Bytes *payload)
{
    pollfd waiting{m_socket, POLLIN, 0};
    if (::poll(&waiting, 1, timeoutMs) <= 0) // timeout, or interrupted by Ctrl+C
        return false;

    std::uint8_t buffer[65536];
    const ssize_t size = ::recv(m_socket, buffer, sizeof buffer, 0);
    if (size < ssize_t(IdSize))
        return false; // not one of ours
    *id = 0;
    for (std::size_t i = 0; i < IdSize; ++i)
        *id |= std::uint32_t(buffer[i]) << (8 * i);
    payload->assign(buffer + IdSize, buffer + size);
    return true;
}
