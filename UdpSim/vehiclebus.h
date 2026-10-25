#ifndef UDPSIM_VEHICLEBUS_H
#define UDPSIM_VEHICLEBUS_H

#include "bytes.h"

#include <cstdint>
#include <functional>

// The bus traffic of examples/Vehicle.ffxml, written by hand. UdpSim sends the
// cyclic messages that EngineECU and BodyECU transmit, with values from a simple
// drive (engine revving, speed rising and falling, coolant warming up, fuel going down):
//
//   0x100 EngineData   every 50 ms   EngineSpeed, CoolantTemp, ThrottlePosition
//   0x110 VehicleData  every 100 ms  VehicleSpeed (Motorola), Gear
//   0x2A0 Dashboard    every 200 ms  Ignition, FuelLevel, AmbientTemp
//
// and reads the one the tester sends:
//
//   0x350 BodyCommands               HeadLamps, WiperSpeed, InteriorLight
class VehicleBus
{
public:
    using Send = std::function<void(std::uint32_t id, const Bytes &payload)>;

    struct BodyCommands
    {
        bool headLamps = false;
        int wiperSpeed = 0;         // 0 Off, 1 Interval, 2 Slow, 3 Fast
        double interiorLight = 0.0; // %
    };

    static const std::uint32_t BodyCommandsId = 0x350;

    explicit VehicleBus(Send send);

    // Sends the messages that are due.
    void update();
    Clock::time_point nextDeadline() const;

    // Decodes a BodyCommands payload; false if it is too short.
    static bool decodeBodyCommands(const Bytes &payload, BodyCommands *commands);

private:
    Bytes engineData(double seconds) const;
    Bytes vehicleData(double seconds) const;
    Bytes dashboard(double seconds) const;

    Send m_send;
    Clock::time_point m_start;
    Clock::time_point m_nextEngineData;
    Clock::time_point m_nextVehicleData;
    Clock::time_point m_nextDashboard;
};

#endif // UDPSIM_VEHICLEBUS_H
