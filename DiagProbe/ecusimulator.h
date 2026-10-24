#ifndef ECUSIMULATOR_H
#define ECUSIMULATOR_H

#include "ffcore/database.h"
#include "usccore/database.h"

#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QSet>

class QTimer;
class UdpBus;

// A simulated ECU inside DiagProbe, so the tester can be tried without hardware.
// It talks over its own UDP socket, exactly as a real ECU would:
//  - a UDS server built from the .uscxml: sessions, ECU resets, security access
//    (with computeKey() from diagdefaults.h), DIDs kept in memory, the NRCs of the
//    description (wrong session, locked, wrong length, ...) and the S3 timeout.
//    Writes in the programming session answer 0x78 (response pending) first, to
//    show P2* at work.
//  - bus traffic from the .ffxml: every cyclic message that the "Tester" node does
//    not transmit, with slowly moving signal values.
class EcuSimulator : public QObject
{
    Q_OBJECT

public:
    explicit EcuSimulator(QObject *parent = nullptr);

    // Listens on port and answers to testerAddress:testerPort. Either database may be nullptr.
    bool start(const usc::Database *description, const ff::Database *database, quint16 port,
               const QHostAddress &testerAddress, quint16 testerPort, int idSize, QString *errorMessage);
    void stop();

private:
    void receive(quint32 id, const QByteArray &payload);
    void handleRequest(const QByteArray &request);
    // The response to a request; empty if the ECU stays silent.
    QByteArray respond(const QByteArray &request);
    QByteArray diagnosticSessionControl(const QByteArray &request);
    QByteArray ecuReset(const QByteArray &request);
    QByteArray readDataByIdentifier(const QByteArray &request);
    QByteArray securityAccess(const QByteArray &request);
    QByteArray writeDataByIdentifier(const QByteArray &request);
    QByteArray testerPresent(const QByteArray &request);

    bool inSession(const QVector<int> &sessionIds) const;
    // True if no level is needed or any of them is unlocked.
    bool isUnlocked(const QVector<int> &securityLevelIds) const;
    void enterDefaultSession();
    void sendResponse(const QByteArray &response);
    void sendBusTraffic();

    const usc::Database *m_description = nullptr;
    const ff::Database *m_database = nullptr;
    UdpBus *m_bus;
    quint32 m_requestId = 0;
    quint32 m_responseId = 0;

    int m_sessionId = 0;
    QSet<int> m_unlockedLevelIds;
    QHash<int, QByteArray> m_seeds;      // SecurityLevel::id -> seed sent, waiting for its key
    QHash<int, int> m_failedAttempts;    // SecurityLevel::id -> wrong keys so far
    QHash<int, qint64> m_lockedOutUntil; // SecurityLevel::id -> time (ms) until seeds are refused
    QHash<quint16, QByteArray> m_dataRecords; // DID -> current data record
    qint64 m_silentUntil = 0;            // ms; the ECU is resetting and answers nothing
    QTimer *m_s3Timer;

    QElapsedTimer m_clock;
    QTimer *m_busTimer;
    QHash<int, qint64> m_nextTransmit; // ff Message::id -> time (ms) it is sent next
};

#endif // ECUSIMULATOR_H
