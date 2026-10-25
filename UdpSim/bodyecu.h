#ifndef UDPSIM_BODYECU_H
#define UDPSIM_BODYECU_H

#include "bytes.h"

#include <cstdint>
#include <functional>
#include <map>
#include <random>

// The UDS server of the BodyECU, written by hand from examples/BodyECU.uscxml:
//
//   Sessions (0x10)    0x01 Default      P2 50 ms,  P2* 5000 ms
//                      0x02 Programming  P2 100 ms, P2* 10000 ms
//                      0x03 Extended     P2 50 ms,  P2* 5000 ms
//   Resets (0x11)      0x01 Hard Reset      in every session
//                      0x03 Soft Reset      in Extended
//                      0x04 Rapid Shutdown  in Extended, Extended level unlocked; power down time 10 s
//   Security (0x27)    0x01/0x02 Extended     in Extended,    4-byte seed and key, 3 attempts
//                      0x11/0x12 Programming  in Programming, 16-byte seed and key, 3 attempts
//   DIDs (0x22 / 0x2E) see the table in bodyecu.cpp
//
// After a reset the ECU is back in the default session and locked. Without a
// request for S3 (5 s) it falls back to the default session by itself.
// Writes in the programming session answer 0x78 (response pending) first.
//
// The security key algorithm is the one of DiagProbe's example plugin (examples/SeedKey):
//     key[i] = seed[i mod seedLength] XOR 0x5A XOR i
class BodyEcu
{
public:
    using Send = std::function<void(const Bytes &response)>;

    explicit BodyEcu(Send send);

    // A request arrived (message ID 0x7E0).
    void handleRequest(const Bytes &request);
    // Does what is due: the S3 timeout, a delayed response.
    void update();
    // How long update() can wait at most.
    Clock::time_point nextDeadline() const;
    // True for a moment after a reset: the ECU answers nothing and sends nothing.
    bool isResetting() const;

    // The interior light the tester commands with BodyCommands, in %; DID 0x0500 reads it.
    void setInteriorLight(double percent);

private:
    enum Session : std::uint8_t { Default = 0x01, Programming = 0x02, Extended = 0x03 };
    enum Level { None = 0, ExtendedLevel = 1, ProgrammingLevel = 2 };

    Bytes diagnosticSessionControl(const Bytes &request);
    Bytes ecuReset(const Bytes &request);
    Bytes readDataByIdentifier(const Bytes &request);
    Bytes securityAccess(const Bytes &request);
    Bytes writeDataByIdentifier(const Bytes &request);
    Bytes testerPresent(const Bytes &request);

    void enterDefaultSession(); // also locks every level

    Send m_send;
    Session m_session = Default;
    bool m_unlocked[3] = {true, false, false}; // by Level; None counts as always unlocked
    Bytes m_seed[3];                           // seed sent, waiting for its key; empty = none
    int m_failedAttempts[3] = {};
    Clock::time_point m_lockedOutUntil[3] = {};
    std::map<std::uint16_t, Bytes> m_records; // DID -> data record
    Clock::time_point m_lastRequest;
    Clock::time_point m_silentUntil;
    Bytes m_delayedResponse; // sent at m_delayedAt; empty = none
    Clock::time_point m_delayedAt;
    std::mt19937 m_random{std::random_device{}()};
};

#endif // UDPSIM_BODYECU_H
