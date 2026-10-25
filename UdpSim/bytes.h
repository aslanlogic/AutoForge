#ifndef UDPSIM_BYTES_H
#define UDPSIM_BYTES_H

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using Bytes = std::vector<std::uint8_t>;
using Clock = std::chrono::steady_clock;

// "62 F1 90 57"
inline std::string toHex(const Bytes &bytes)
{
    std::string text;
    char byte[4];
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        std::snprintf(byte, sizeof byte, i == 0 ? "%02X" : " %02X", bytes[i]);
        text += byte;
    }
    return text;
}

#endif // UDPSIM_BYTES_H
