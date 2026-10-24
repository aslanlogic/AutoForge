#ifndef RESETTYPE_H
#define RESETTYPE_H

#include "uds.h"

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace usc {

// powerDownTime value meaning "not available".
const int PowerDownTimeNotAvailable = 0xFF;

// One reset of ECUReset (0x11). The tester asks for it by sending its resetType
// sub-function; the ECU answers, then resets. Afterwards it is back in the
// default session with security locked.
struct ResetType
{
    int id = 0; // unique, assigned by Database when added; 0 = not added yet
    QString name;
    quint8 resetType = 0x01; // sub-function; see isValidResetType()
    // Session::id of every session the reset is allowed in. In any other
    // session the ECU refuses the request.
    QVector<int> sessionIds;
    // SecurityLevel::id of the levels that allow the reset: it is accepted if any
    // of them is unlocked (otherwise NRC 0x33). Empty = no security needed.
    QVector<int> securityLevelIds;
    // Only for EnableRapidPowerShutDown (0x04): reported in the positive response,
    // in seconds, 0-254; 0xFF = not available.
    int powerDownTime = PowerDownTimeNotAvailable;
    QString comment;
};

// Equal if every field is equal. Used to tell whether an edit changed anything.
inline bool operator==(const ResetType &a, const ResetType &b)
{
    return a.id == b.id && a.name == b.name && a.resetType == b.resetType && a.sessionIds == b.sessionIds
        && a.securityLevelIds == b.securityLevelIds && a.powerDownTime == b.powerDownTime
        && a.comment == b.comment;
}

// The reset type whose positive response carries powerDownTime.
const quint8 EnableRapidPowerShutDown = 0x04;

// resetType sub-functions allowed by ISO 14229-1: 0x01-0x05 (see
// resetTypeDescription()), 0x40-0x5F vehicle manufacturer specific and
// 0x60-0x7E system supplier specific.
inline bool isValidResetType(quint8 resetType)
{
    return (resetType >= 0x01 && resetType <= 0x05)
        || (resetType >= 0x40 && resetType <= 0x7E);
}

// What ISO 14229-1 calls a resetType, e.g. "Hard Reset" for 0x01.
inline QString resetTypeDescription(quint8 resetType)
{
    switch (resetType) {
    case 0x01: return "Hard Reset";
    case 0x02: return "Key Off On Reset";
    case 0x03: return "Soft Reset";
    case 0x04: return "Enable Rapid Power Shut Down";
    case 0x05: return "Disable Rapid Power Shut Down";
    }
    if (resetType >= 0x40 && resetType <= 0x5F)
        return "Vehicle Manufacturer Specific";
    if (resetType >= 0x60 && resetType <= 0x7E)
        return "System Supplier Specific";
    return "Reserved";
}

} // namespace usc

#endif // RESETTYPE_H
