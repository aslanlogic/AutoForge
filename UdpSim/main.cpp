// UdpSim: the BodyECU of examples/BodyECU.uscxml on the bus of examples/Vehicle.ffxml,
// so DiagProbe can be tried without hardware.
//
// Usage: UdpSim [--tester <IPv4 address>]
//
// As in the example files, the ECU listens on UDP port 5000 and sends to the tester
// at port 5001 (on this PC unless --tester says otherwise). UDS requests come on
// ID 0x7E0, responses go out on 0x7E8. Ctrl+C stops it.

#include "bodyecu.h"
#include "udpsocket.h"
#include "vehiclebus.h"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

const std::uint16_t EcuPort = 5000;    // remotePort in the example files
const std::uint16_t TesterPort = 5001; // localPort in the example files
const std::uint32_t RequestId = 0x7E0;
const std::uint32_t ResponseId = 0x7E8;

std::atomic<bool> stopRequested{false};

void requestStop(int)
{
    stopRequested = true;
}

const Clock::time_point Start = Clock::now();

void log(const char *direction, const Bytes &message)
{
    const double seconds = std::chrono::duration<double>(Clock::now() - Start).count();
    std::printf("%9.3f  %s  %s\n", seconds, direction, toHex(message).c_str());
    std::fflush(stdout);
}

bool operator!=(const VehicleBus::BodyCommands &a, const VehicleBus::BodyCommands &b)
{
    return a.headLamps != b.headLamps || a.wiperSpeed != b.wiperSpeed || a.interiorLight != b.interiorLight;
}

} // namespace

int main(int argc, char *argv[])
{
    std::string testerAddress = "127.0.0.1";
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--tester") == 0 && i + 1 < argc) {
            testerAddress = argv[++i];
        } else {
            std::fprintf(stderr, "Usage: %s [--tester <IPv4 address>]\n", argv[0]);
            return 2;
        }
    }

    UdpSocket socket;
    std::string error;
    if (!socket.open(EcuPort, testerAddress, TesterPort, &error)) {
        std::fprintf(stderr, "UdpSim: %s\n", error.c_str());
        return 1;
    }
    std::signal(SIGINT, requestStop);
    std::signal(SIGTERM, requestStop);

    BodyEcu ecu([&socket](const Bytes &response) {
        socket.send(ResponseId, response);
        log("Tx", response);
    });
    VehicleBus bus([&socket](std::uint32_t id, const Bytes &payload) { socket.send(id, payload); });
    VehicleBus::BodyCommands lastCommands;

    std::printf("UdpSim: BodyECU on UDP port %u, tester at %s:%u, requests 0x7E0, responses 0x7E8. "
                "Ctrl+C stops.\n", EcuPort, testerAddress.c_str(), TesterPort);
    std::fflush(stdout);

    while (!stopRequested) {
        // Wait for a message, but no longer than until something is due.
        const auto deadline = std::min(ecu.nextDeadline(), bus.nextDeadline());
        const auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        std::uint32_t id = 0;
        Bytes payload;
        if (socket.receive(int(std::max<long long>(0, wait)), &id, &payload)) {
            if (id == RequestId && !ecu.isResetting()) {
                log("Rx", payload);
                ecu.handleRequest(payload);
            } else if (id == VehicleBus::BodyCommandsId) {
                VehicleBus::BodyCommands commands;
                if (VehicleBus::decodeBodyCommands(payload, &commands)) {
                    ecu.setInteriorLight(commands.interiorLight); // DID 0x0500 reads what the tester commands
                    if (commands != lastCommands) {
                        static const char *const Wiper[] = {"Off", "Interval", "Slow", "Fast"};
                        std::printf("           BodyCommands: head lamps %s, wiper %s, interior light %.1f %%\n",
                                    commands.headLamps ? "On" : "Off", Wiper[commands.wiperSpeed],
                                    commands.interiorLight);
                        std::fflush(stdout);
                        lastCommands = commands;
                    }
                }
            }
        }
        ecu.update();
        if (!ecu.isResetting())
            bus.update();
    }
    std::printf("UdpSim: stopped.\n");
    return 0;
}
