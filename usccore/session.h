#ifndef SESSION_H
#define SESSION_H

#include "uds.h"

#include <QString>
#include <QtGlobal>

namespace usc {

// One diagnostic session of DiagnosticSessionControl (0x10). The tester enters
// it by sending its sessionType sub-function; the ECU confirms with a positive
// response that reports the session's P2 and P2* timing.
struct Session
{
    int id = 0; // unique, assigned by Database when added; 0 = not added yet
    QString name;
    quint8 sessionType = 0x01; // sub-function; see isValidSessionType()
    // Reported in the positive response: P2 in 1 ms steps, P2* in 10 ms steps,
    // both as 16-bit numbers, so at most 65535 ms and 655350 ms.
    int p2 = 50;       // ms, P2server_max: until the first response
    int p2Star = 5000; // ms, P2*server_max: after each NRC 0x78 (response pending)
    QString comment;
};

// Equal if every field is equal. Used to tell whether an edit changed anything.
inline bool operator==(const Session &a, const Session &b)
{
    return a.id == b.id && a.name == b.name && a.sessionType == b.sessionType && a.p2 == b.p2
        && a.p2Star == b.p2Star && a.comment == b.comment;
}

// The default session. Every ECU supports it and every document has it.
const quint8 DefaultSessionType = 0x01;

// sessionType sub-functions allowed by ISO 14229-1: 0x01 default, 0x02 programming,
// 0x03 extended diagnostic, 0x04 safety system, 0x40-0x5F vehicle manufacturer
// specific and 0x60-0x7E system supplier specific.
inline bool isValidSessionType(quint8 sessionType)
{
    return (sessionType >= 0x01 && sessionType <= 0x04)
        || (sessionType >= 0x40 && sessionType <= 0x7E);
}

} // namespace usc

#endif // SESSION_H
