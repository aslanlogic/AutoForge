#include "vehiclebus.h"

#include <algorithm>
#include <cmath>

namespace {

using namespace std::chrono_literals;

const double Pi = 3.14159265358979323846;

// A slow wave between low and high with this period.
double wave(double seconds, double period, double low, double high)
{
    return low + (high - low) * (0.5 - 0.5 * std::cos(2 * Pi * seconds / period));
}

// physical = raw * factor + offset, so raw = (physical - offset) / factor, rounded.
long raw(double physical, double factor, double offset = 0.0)
{
    return std::lround((physical - offset) / factor);
}

double vehicleSpeed(double seconds) // km/h, 0..130 over 40 s
{
    return wave(seconds, 40.0, 0.0, 130.0);
}

} // namespace

VehicleBus::VehicleBus(Send send)
    : m_send(std::move(send))
    , m_start(Clock::now())
    , m_nextEngineData(m_start)
    , m_nextVehicleData(m_start)
    , m_nextDashboard(m_start)
{
}

void VehicleBus::update()
{
    const auto now = Clock::now();
    const double seconds = std::chrono::duration<double>(now - m_start).count();
    if (now >= m_nextEngineData) {
        m_send(0x100, engineData(seconds));
        m_nextEngineData += 50ms;
    }
    if (now >= m_nextVehicleData) {
        m_send(0x110, vehicleData(seconds));
        m_nextVehicleData += 100ms;
    }
    if (now >= m_nextDashboard) {
        m_send(0x2A0, dashboard(seconds));
        m_nextDashboard += 200ms;
    }
    // After a pause (a reset), do not send all the missed ones at once.
    m_nextEngineData = std::max(m_nextEngineData, now);
    m_nextVehicleData = std::max(m_nextVehicleData, now);
    m_nextDashboard = std::max(m_nextDashboard, now);
}

Clock::time_point VehicleBus::nextDeadline() const
{
    return std::min({m_nextEngineData, m_nextVehicleData, m_nextDashboard});
}

// EngineData, 8 bytes:
//   EngineSpeed       start bit 0,  16 bit Intel,  0.25 rpm          -> bytes 0..1, low byte first
//   CoolantTemp       start bit 16,  8 bit Intel,  1 °C, offset -40  -> byte 2
//   ThrottlePosition  start bit 24,  8 bit Intel,  0.4 %             -> byte 3
Bytes VehicleBus::engineData(double seconds) const
{
    const double speed = vehicleSpeed(seconds);
    const double rpm = 800.0 + speed * 35.0 + wave(seconds, 3.0, 0.0, 400.0); // revs with the speed
    const double coolant = std::min(90.0, 20.0 + seconds * 0.5);              // warms up, then holds
    const double throttle = wave(seconds, 7.0, 0.0, 100.0);

    Bytes payload(8, 0);
    const long engineSpeed = raw(rpm, 0.25);
    payload[0] = std::uint8_t(engineSpeed);
    payload[1] = std::uint8_t(engineSpeed >> 8);
    payload[2] = std::uint8_t(raw(coolant, 1.0, -40.0));
    payload[3] = std::uint8_t(raw(throttle, 0.4));
    return payload;
}

// VehicleData, 8 bytes:
//   VehicleSpeed  start bit 7,  16 bit Motorola,  0.01 km/h  -> bytes 0..1, high byte first
//   Gear          start bit 16,  4 bit Intel,     0 P 1 R 2 N 3 D 4 S  -> low half of byte 2
Bytes VehicleBus::vehicleData(double seconds) const
{
    const double speed = vehicleSpeed(seconds);
    const int gear = speed < 1.0 ? 0 : 3; // P when standing, D when driving

    Bytes payload(8, 0);
    const long vehicleSpeedRaw = raw(speed, 0.01);
    payload[0] = std::uint8_t(vehicleSpeedRaw >> 8);
    payload[1] = std::uint8_t(vehicleSpeedRaw);
    payload[2] = std::uint8_t(gear & 0x0F);
    return payload;
}

// Dashboard, 4 bytes:
//   Ignition     start bit 0,   1 bit Intel,           0 Off 1 On  -> bit 0 of byte 0
//   FuelLevel    start bit 8,   8 bit Intel,           0.4 %       -> byte 1
//   AmbientTemp  start bit 16,  8 bit Intel, signed,   0.5 °C      -> byte 2
Bytes VehicleBus::dashboard(double seconds) const
{
    const double fuel = 80.0 - std::fmod(seconds * 0.1, 70.0); // slowly empties, refilled at 10 %
    const double ambient = 18.0 + 3.0 * std::sin(2 * Pi * seconds / 120.0);

    Bytes payload(4, 0);
    payload[0] = 1; // ignition on
    payload[1] = std::uint8_t(raw(fuel, 0.4));
    payload[2] = std::uint8_t(std::int8_t(raw(ambient, 0.5)));
    return payload;
}

// BodyCommands, 3 bytes:
//   HeadLamps      start bit 0,  1 bit Intel,  0 Off 1 On                    -> bit 0 of byte 0
//   WiperSpeed     start bit 1,  2 bit Intel,  0 Off 1 Interval 2 Slow 3 Fast -> bits 1..2 of byte 0
//   InteriorLight  start bit 8,  8 bit Intel,  0.5 %                         -> byte 1
bool VehicleBus::decodeBodyCommands(const Bytes &payload, BodyCommands *commands)
{
    if (payload.size() < 2)
        return false;
    commands->headLamps = payload[0] & 0x01;
    commands->wiperSpeed = (payload[0] >> 1) & 0x03;
    commands->interiorLight = payload[1] * 0.5;
    return true;
}
