#ifndef SECURITYLEVEL_H
#define SECURITYLEVEL_H

#include "uds.h"

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace usc {

// One security level of SecurityAccess (0x27). The tester unlocks it by sending
// requestSeed, computing a key from the seed and sending it back with the
// sendKey sub-function (see sendKey() below).
struct SecurityLevel
{
    int id = 0; // unique, assigned by Database when added; 0 = not added yet
    QString name;
    quint8 requestSeed = 0x01; // sub-function; odd, see isValidRequestSeed()
    int seedLength = 4;  // in bytes
    int keyLength = 4;   // in bytes
    int maxAttempts = 3; // wrong keys accepted before the ECU refuses further tries
    // Session::id of every session the level can be unlocked in. In any other
    // session the ECU refuses the request.
    QVector<int> sessionIds;
    QString comment;
};

// Equal if every field is equal. Used to tell whether an edit changed anything.
inline bool operator==(const SecurityLevel &a, const SecurityLevel &b)
{
    return a.id == b.id && a.name == b.name && a.requestSeed == b.requestSeed && a.seedLength == b.seedLength
        && a.keyLength == b.keyLength && a.maxAttempts == b.maxAttempts && a.sessionIds == b.sessionIds
        && a.comment == b.comment;
}

// The sendKey sub-function that belongs to a requestSeed sub-function.
inline quint8 sendKey(quint8 requestSeed)
{
    return requestSeed + 1;
}

// requestSeed sub-functions allowed by ISO 14229-1: odd values in
// 0x01-0x41 (security levels), 0x5F (ISO 26021) and 0x61-0x7D (supplier specific).
inline bool isValidRequestSeed(quint8 requestSeed)
{
    if (requestSeed % 2 == 0)
        return false;
    return (requestSeed >= 0x01 && requestSeed <= 0x41)
        || requestSeed == 0x5F
        || (requestSeed >= 0x61 && requestSeed <= 0x7D);
}

} // namespace usc

#endif // SECURITYLEVEL_H
