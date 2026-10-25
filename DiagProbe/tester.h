#ifndef TESTER_H
#define TESTER_H

#include "ffcore/database.h"
#include "usccore/database.h"

#include <QElapsedTimer>
#include <QObject>
#include <QSet>

class KeyAlgorithm;
class QTimer;
class UdpBus;

// The measurement: DiagProbe's side of the bus. It sends UDS requests and waits
// for their responses (P2, or P2* after 0x78 response pending), keeps track of
// the ECU's session and unlocked security levels as the responses tell them,
// keeps a session alive with TesterPresent and sends bus messages. While it runs
// it reports every message it sends or receives.
//
// Addressing comes from the .uscxml's UDP settings (the .ffxml's if there is no
// .uscxml): it listens on localPort and sends to remoteAddress:remotePort. UDS
// requests use the requestId, responses the responseId; bus messages use their frame IDs.
class Tester : public QObject
{
    Q_OBJECT

public:
    explicit Tester(QObject *parent = nullptr);

    // Either may be nullptr. Only while stopped.
    void setDatabases(const usc::Database *description, const ff::Database *database);
    const usc::Database *description() const { return m_description; }
    const ff::Database *database() const { return m_database; }

    bool start(QString *errorMessage);
    void stop();
    bool isRunning() const { return m_running; }
    // Seconds since the measurement started.
    double time() const;

    // True while a request waits for its response; no other request can be sent then.
    bool isBusy() const { return !m_pendingRequest.isEmpty(); }
    // Sends a UDS request. False if not running or busy.
    bool sendRequest(const QByteArray &request);
    // The key algorithm plugin unlock() uses; nullptr or not loaded = none.
    void setKeyAlgorithm(const KeyAlgorithm *keyAlgorithm) { m_keyAlgorithm = keyAlgorithm; }
    // Requests a seed for the level, computes the key with the key algorithm and
    // sends it. False, and why, if it cannot start.
    bool unlock(int securityLevelId, QString *errorMessage = nullptr);

    void setTesterPresentEnabled(bool enabled);
    bool isTesterPresentEnabled() const { return m_testerPresentEnabled; }

    // The ECU's state as the responses told it.
    int sessionId() const { return m_sessionId; }
    const QSet<int> &unlockedLevelIds() const { return m_unlockedLevelIds; }
    bool isUnlocked(const QVector<int> &securityLevelIds) const; // true if none needed
    QString lastResponse() const { return m_lastResponse; }
    // Seconds until the ECU falls back to the default session, or -1 in the default session.
    double s3Remaining() const;

    // Sends a bus message.
    void sendFrame(quint32 id, const QByteArray &payload);

    quint64 receivedCount() const { return m_receivedCount; }
    quint64 sentCount() const { return m_sentCount; }

signals:
    void runningChanged(bool running);
    void frameSeen(double time, quint32 id, const QByteArray &payload, bool transmitted);
    void requestSent(double time, const QByteArray &request);
    void responseReceived(double time, const QByteArray &request, const QByteArray &response);
    void requestTimedOut(double time, const QByteArray &request);
    // unlock() got a seed but the key algorithm could not compute its key; no key is sent.
    void keyFailed(double time, const QString &errorMessage);
    void stateChanged();

private:
    void transmit(quint32 id, const QByteArray &payload);
    void receive(quint32 id, const QByteArray &payload);
    void handleResponse(const QByteArray &response);
    void responseTimeout();
    void sendTesterPresent();
    void checkS3();
    void enterDefaultSession();
    int currentP2() const;
    int currentP2Star() const;

    const usc::Database *m_description = nullptr;
    const ff::Database *m_database = nullptr;
    UdpBus *m_bus;
    QElapsedTimer m_clock;
    bool m_running = false;
    quint32 m_requestId = 0x7E0;
    quint32 m_responseId = 0x7E8;

    QByteArray m_pendingRequest;
    QTimer *m_responseTimer;
    int m_unlockLevelId = 0; // the level unlock() is busy with
    const KeyAlgorithm *m_keyAlgorithm = nullptr;

    int m_sessionId = 0;
    QSet<int> m_unlockedLevelIds;
    QString m_lastResponse;
    double m_lastRequestTime = 0.0;
    bool m_testerPresentEnabled = true;
    QTimer *m_testerPresentTimer;
    QTimer *m_s3Timer;

    quint64 m_receivedCount = 0;
    quint64 m_sentCount = 0;
};

#endif // TESTER_H
