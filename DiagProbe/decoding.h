#ifndef DECODING_H
#define DECODING_H

#include "ffcore/database.h"
#include "usccore/database.h"

#include <QByteArray>
#include <QString>
#include <QVector>

#include <initializer_list>

// Turning payloads into readable values and back, for bus messages (.ffxml) and
// UDS messages (.uscxml). The two cores have types with the same names, so this
// file always writes ff:: and usc:: out.

// One value read from a payload: a signal of a message or a placement in a DID's data record.
struct DecodedValue
{
    QString name;
    bool isNumber = true; // false for ASCII and Bytes
    QString raw;          // decimal for numbers, hex for ASCII and Bytes
    double physical = 0.0; // numbers only
    QString physicalText;  // the number, or the text of ASCII and Bytes
    QString unit;
    QString valueText;     // from the value table; empty if none or no entry
};

// Bytes from numbers: bytesOf({0x22, 0xF1, 0x90}). Each value is cut to its lowest byte.
inline QByteArray bytesOf(std::initializer_list<int> values)
{
    QByteArray bytes;
    for (int value : values)
        bytes.append(char(value & 0xFF));
    return bytes;
}

// "62 F1 90 57"
QString hexBytes(const QByteArray &bytes);
// Hex text with or without spaces ("62F1", "62 f1") to bytes; false if it is not hex.
bool parseHex(const QString &text, QByteArray *bytes);
// A physical value as short as possible: 2 rather than 2.0000000000000004.
QString formatNumber(double value);

// --- Bus messages ---

QVector<DecodedValue> decodeMessage(const ff::Database &database, const ff::Message &message,
                                    const QByteArray &payload);
double decodeSignal(const ff::Signal &signal, int startBit, const QByteArray &payload);
void encodeSignal(const ff::Signal &signal, int startBit, double physical, QByteArray *payload);
const ff::Message *findMessageByFrameId(const ff::Database &database, quint32 frameId);

// --- UDS ---

const usc::Session *findSessionByType(const usc::Database &description, quint8 sessionType);
const usc::Session *defaultSession(const usc::Database &description);
const usc::ResetType *findResetTypeByValue(const usc::Database &description, quint8 resetType);
const usc::SecurityLevel *findLevelByRequestSeed(const usc::Database &description, quint8 requestSeed);
const usc::DataIdentifier *findDid(const usc::Database &description, quint16 did);

QVector<DecodedValue> decodeDataRecord(const usc::Database &description, const usc::DataIdentifier &did,
                                       const QByteArray &record);
// Writes one placement's value into a data record. `text` is a physical number or a
// value table description for numbers, the text for ASCII (padded with spaces) and
// hex for Bytes. Returns false and says why if the text does not fit.
bool encodePlacement(const usc::Database &description, const usc::DataTypeMapping &mapping,
                     const QString &text, QByteArray *record, QString *errorMessage);

QString serviceName(quint8 serviceId);    // "ReadDataByIdentifier"
QString nrcName(quint8 nrc);              // "requestOutOfRange"
QString nrcText(quint8 nrc);              // "0x31 requestOutOfRange"
bool isNegativeResponse(const QByteArray &response);
// What a request or response means, e.g. "Read 0xF190 VIN" or "Positive: Extended, P2 50 ms, P2* 5000 ms".
// The description may be nullptr; then only the bytes are explained.
QString describeUds(const usc::Database *description, const QByteArray &message);

#endif // DECODING_H
