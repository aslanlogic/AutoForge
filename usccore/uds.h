#ifndef USCCORE_UDS_H
#define USCCORE_UDS_H

#include <QString>
#include <QtGlobal>

namespace usc {

// Small helpers shared by all UDS services.

// Sub-functions are shown and saved as two-digit hex: "0x01".
inline QString subFunctionToText(quint8 subFunction)
{
    return "0x" + QString("%1").arg(uint(subFunction), 2, 16, QChar('0')).toUpper();
}

} // namespace usc

#endif // USCCORE_UDS_H
