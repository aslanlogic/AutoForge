#include "bodyecu.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

using namespace std::chrono_literals;

const auto S3Time = 5000ms;         // back to the default session without requests
const auto ResetTime = 500ms;       // silent after an ECUReset
const auto LockoutTime = 10000ms;   // seeds refused after too many wrong keys
const auto SlowWriteTime = 1500ms;  // a write in the programming session
const int MaxAttempts = 3;          // both levels

Bytes negative(std::uint8_t serviceId, std::uint8_t nrc)
{
    return {0x7F, serviceId, nrc};
}

bool isNegative(const Bytes &response)
{
    return !response.empty() && response[0] == 0x7F;
}

Bytes text(const std::string &value)
{
    return Bytes(value.begin(), value.end());
}

// What each DID of BodyECU.uscxml allows. Reading needs no security level for any of them.
struct DidRule
{
    std::uint16_t did;
    std::size_t length;     // data record, bytes
    bool readInDefault;     // every DID is readable in Programming and Extended
    std::uint8_t writeSession; // 0 = not writable
    int writeLevel;         // the level a write needs
};

//                   DID     length  read in Default  write in          level
const DidRule Dids[] = {
    {0xF190, 17, true,  0x02, 2}, // VIN: Programming, Programming level
    {0xF187, 10, true,  0,    0}, // SparePartNumber (10 raw bytes)
    {0xF195,  8, true,  0,    0}, // SoftwareVersion (8 ASCII characters)
    {0x0100,  4, true,  0,    0}, // TyrePressures: 4 x 0.02 bar, Intel bytes 0..3
    {0x0200,  3, true,  0x03, 1}, // Odometer: 24 bit, 0.1 km, Motorola: bytes 0..2 big-endian
    {0x0300,  1, true,  0,    0}, // DoorStatus: bits 0..3 doors (1 = Open), bits 4..7 Gear (0 P .. 4 S)
    {0x0400,  2, false, 0,    0}, // BatteryVoltage: 16 bit, 0.001 V, Motorola; Extended only
    {0x0500,  1, true,  0x03, 1}, // InteriorLight: 0.5 %; Extended, Extended level
};

const DidRule *findDid(std::uint16_t did)
{
    for (const DidRule &rule : Dids) {
        if (rule.did == did)
            return &rule;
    }
    return nullptr;
}

} // namespace

BodyEcu::BodyEcu(Send send)
    : m_send(std::move(send))
{
    // Values the ECU starts with.
    m_records[0xF190] = text("WAF0BODYECU000001");
    m_records[0xF187] = {0x0B, 0xE0, 0xC0, 0x00, 0x12, 0x34, 0x56, 0x78, 0x9A, 0x01};
    m_records[0xF195] = text("SW 1.4.2");
    m_records[0x0100] = {115, 116, 112, 113};                // 2.30, 2.32, 2.24, 2.26 bar
    m_records[0x0200] = {0x01, 0xE2, 0x40};                  // 12345.6 km = 123456
    m_records[0x0300] = {0x01};                              // driver door open, gear P
    m_records[0x0400] = {0x31, 0x38};                        // 12.600 V = 12600
    m_records[0x0500] = {0};                                 // light off
    m_lastRequest = Clock::now();
}

void BodyEcu::handleRequest(const Bytes &request)
{
    if (request.empty() || isResetting())
        return;
    m_lastRequest = Clock::now();

    const std::uint8_t serviceId = request[0];
    Bytes response;
    switch (serviceId) {
    case 0x10: response = diagnosticSessionControl(request); break;
    case 0x11: response = ecuReset(request); break;
    case 0x22: response = readDataByIdentifier(request); break;
    case 0x27: response = securityAccess(request); break;
    case 0x2E: response = writeDataByIdentifier(request); break;
    case 0x3E: response = testerPresent(request); break;
    default:   response = negative(serviceId, 0x11); break; // serviceNotSupported
    }

    // Writing flash memory takes a while: ask the tester to wait (P2*), then answer.
    if (serviceId == 0x2E && m_session == Programming && !isNegative(response)) {
        m_send(negative(serviceId, 0x78));
        m_delayedResponse = response;
        m_delayedAt = Clock::now() + SlowWriteTime;
        return;
    }

    // suppressPosRspMsgIndicationBit: a positive response is not sent.
    const bool hasSubFunction = serviceId == 0x10 || serviceId == 0x11 || serviceId == 0x27 || serviceId == 0x3E;
    const bool suppress = hasSubFunction && request.size() >= 2 && (request[1] & 0x80);
    if (!(suppress && !isNegative(response)))
        m_send(response);

    if (serviceId == 0x11 && !isNegative(response)) {
        enterDefaultSession();
        m_silentUntil = Clock::now() + ResetTime;
    }
}

void BodyEcu::update()
{
    const auto now = Clock::now();
    if (!m_delayedResponse.empty() && now >= m_delayedAt) {
        m_send(m_delayedResponse);
        m_delayedResponse.clear();
    }
    if (m_session != Default && now - m_lastRequest >= S3Time)
        enterDefaultSession();
}

Clock::time_point BodyEcu::nextDeadline() const
{
    Clock::time_point deadline = Clock::now() + 1s; // nothing due: check again in a while
    if (m_session != Default)
        deadline = std::min(deadline, m_lastRequest + S3Time);
    if (!m_delayedResponse.empty())
        deadline = std::min(deadline, m_delayedAt);
    return deadline;
}

bool BodyEcu::isResetting() const
{
    return Clock::now() < m_silentUntil;
}

void BodyEcu::setInteriorLight(double percent)
{
    m_records[0x0500] = {std::uint8_t(std::lround(std::clamp(percent, 0.0, 100.0) / 0.5))};
}

Bytes BodyEcu::diagnosticSessionControl(const Bytes &request)
{
    if (request.size() != 2)
        return negative(0x10, 0x13);
    const std::uint8_t sessionType = request[1] & 0x7F;
    int p2 = 0, p2Star = 0;
    switch (sessionType) {
    case Default:     p2 = 50;  p2Star = 5000;  break;
    case Programming: p2 = 100; p2Star = 10000; break;
    case Extended:    p2 = 50;  p2Star = 5000;  break;
    default:          return negative(0x10, 0x12); // subFunctionNotSupported
    }
    enterDefaultSession(); // a session change locks the ECU again
    m_session = Session(sessionType);
    const int p2StarSteps = p2Star / 10; // reported in 10 ms steps
    return {0x50, sessionType, std::uint8_t(p2 >> 8), std::uint8_t(p2), std::uint8_t(p2StarSteps >> 8),
            std::uint8_t(p2StarSteps)};
}

Bytes BodyEcu::ecuReset(const Bytes &request)
{
    if (request.size() != 2)
        return negative(0x11, 0x13);
    const std::uint8_t resetType = request[1] & 0x7F;
    switch (resetType) {
    case 0x01: // Hard Reset: in every session
        return {0x51, resetType};
    case 0x03: // Soft Reset: in Extended
        if (m_session != Extended)
            return negative(0x11, 0x7E);
        return {0x51, resetType};
    case 0x04: // Rapid Shutdown: in Extended with the Extended level
        if (m_session != Extended)
            return negative(0x11, 0x7E);
        if (!m_unlocked[ExtendedLevel])
            return negative(0x11, 0x33);
        return {0x51, resetType, 10}; // power down time: 10 s
    }
    return negative(0x11, 0x12);
}

Bytes BodyEcu::readDataByIdentifier(const Bytes &request)
{
    if (request.size() != 3)
        return negative(0x22, 0x13);
    const std::uint16_t did = std::uint16_t((request[1] << 8) | request[2]);
    const DidRule *rule = findDid(did);
    if (!rule || (m_session == Default && !rule->readInDefault))
        return negative(0x22, 0x31); // requestOutOfRange
    Bytes response = {0x62, request[1], request[2]};
    const Bytes &record = m_records[did];
    response.insert(response.end(), record.begin(), record.end());
    return response;
}

Bytes BodyEcu::securityAccess(const Bytes &request)
{
    if (request.size() < 2)
        return negative(0x27, 0x13);
    const std::uint8_t subFunction = request[1] & 0x7F;
    Level level = None;
    Session session = Default;
    std::size_t length = 0; // of seed and key
    switch (subFunction) {
    case 0x01: case 0x02: level = ExtendedLevel;    session = Extended;    length = 4;  break;
    case 0x11: case 0x12: level = ProgrammingLevel; session = Programming; length = 16; break;
    default: return negative(0x27, 0x12);
    }
    if (m_session != session)
        return negative(0x27, 0x7E);

    if (subFunction % 2 == 1) { // requestSeed
        if (request.size() != 2)
            return negative(0x27, 0x13);
        if (Clock::now() < m_lockedOutUntil[level])
            return negative(0x27, 0x37); // requiredTimeDelayNotExpired
        Bytes seed(length, 0); // all zero means "already unlocked"
        if (!m_unlocked[level]) {
            std::uniform_int_distribution<int> byte(1, 255); // so a real seed is never all zero
            for (std::uint8_t &value : seed)
                value = std::uint8_t(byte(m_random));
            m_seed[level] = seed;
        }
        Bytes response = {0x67, subFunction};
        response.insert(response.end(), seed.begin(), seed.end());
        return response;
    }

    // sendKey
    if (m_seed[level].empty())
        return negative(0x27, 0x24); // requestSequenceError: no seed asked for
    if (request.size() != 2 + length)
        return negative(0x27, 0x13);
    const Bytes seed = m_seed[level];
    m_seed[level].clear();
    for (std::size_t i = 0; i < length; ++i) {
        const std::uint8_t expected = std::uint8_t(seed[i % seed.size()] ^ 0x5A ^ i);
        if (request[2 + i] != expected) {
            if (++m_failedAttempts[level] >= MaxAttempts) {
                m_failedAttempts[level] = 0;
                m_lockedOutUntil[level] = Clock::now() + LockoutTime;
                return negative(0x27, 0x36); // exceededNumberOfAttempts
            }
            return negative(0x27, 0x35); // invalidKey
        }
    }
    m_failedAttempts[level] = 0;
    m_unlocked[level] = true;
    return {0x67, subFunction};
}

Bytes BodyEcu::writeDataByIdentifier(const Bytes &request)
{
    if (request.size() < 3)
        return negative(0x2E, 0x13);
    const std::uint16_t did = std::uint16_t((request[1] << 8) | request[2]);
    const DidRule *rule = findDid(did);
    if (!rule || rule->writeSession == 0)
        return negative(0x2E, 0x31);
    if (request.size() != 3 + rule->length)
        return negative(0x2E, 0x13);
    if (m_session != rule->writeSession)
        return negative(0x2E, 0x31);
    if (!m_unlocked[rule->writeLevel])
        return negative(0x2E, 0x33); // securityAccessDenied
    m_records[did].assign(request.begin() + 3, request.end());
    return {0x6E, request[1], request[2]};
}

Bytes BodyEcu::testerPresent(const Bytes &request)
{
    if (request.size() != 2)
        return negative(0x3E, 0x13);
    if ((request[1] & 0x7F) != 0x00)
        return negative(0x3E, 0x12);
    return {0x7E, 0x00};
}

void BodyEcu::enterDefaultSession()
{
    m_session = Default;
    m_unlocked[ExtendedLevel] = false;
    m_unlocked[ProgrammingLevel] = false;
    m_seed[ExtendedLevel].clear();
    m_seed[ProgrammingLevel].clear();
}
