#ifndef DIAGNOSTICCONSOLE_H
#define DIAGNOSTICCONSOLE_H

#include <QHash>
#include <QWidget>

namespace usc {
class Database;
} // namespace usc

class QLabel;
class QPushButton;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class Tester;

// The Diagnostic console: everything for talking to the ECU in one window.
//  - Left: the requests the ECU description offers (sessions, resets, DIDs to read
//    or write, security levels to unlock).
//  - Right: the selected request: what it needs (the session and security level,
//    with the NRC the ECU answers otherwise), the values to write, a button to
//    send it, the request and response bytes and the decoded response.
//  - Bottom: the log of every request and response. Clicking a line selects its request.
class DiagnosticConsole : public QWidget
{
    Q_OBJECT

public:
    explicit DiagnosticConsole(Tester *tester, QWidget *parent = nullptr);

    // Fills the request tree from a diagnostic description; nullptr empties it.
    // fileName is shown as the tree's top row.
    void showDescription(const usc::Database *description, const QString &fileName);

    // Selects the request whose key (see keyOfMessage()) this is. False if there is none.
    bool selectRequest(const QString &key);

private:
    enum class RequestKind { None, Session, Reset, Read, Unlock, Write };
    struct Request
    {
        RequestKind kind = RequestKind::None;
        int id = 0; // id of the Session, ResetType, DataIdentifier or SecurityLevel
    };
    // The last request of one kind and what came back.
    struct Exchange
    {
        QByteArray request;
        QByteArray response; // empty if none (yet)
        QString note;        // e.g. "No response"
    };

    Request selectedRequest() const;
    QString keyOf(const Request &request) const;
    // Groups requests by what they ask for: "10:03", "22:F190", "27:01" (seed and key alike).
    static QString keyOfMessage(const QByteArray &message);

    void showSelectedRequest();
    void fillWriteValues(int dataIdentifierId);
    void updatePrerequisites();
    void updateSendButton();
    void updateExchange();
    void send();
    bool buildWriteRequest(int dataIdentifierId, QByteArray *request, QString *errorMessage) const;

    void onRequestSent(double time, const QByteArray &request);
    void onResponseReceived(double time, const QByteArray &request, const QByteArray &response);
    void onRequestTimedOut(double time, const QByteArray &request);
    void addLogRow(double time, const QString &direction, const QByteArray &bytes, const QString &interpretation,
                   const QString &key, const QColor &color);

    Tester *m_tester;
    const usc::Database *m_description = nullptr;

    QTreeWidget *m_requestTree;
    QLabel *m_title;
    QLabel *m_prerequisites;
    QLabel *m_valuesLabel;
    QTableWidget *m_valueTable; // values to write
    QPushButton *m_sendButton;
    QLabel *m_sendStatus;
    QLabel *m_requestBytes;
    QLabel *m_responseBytes;
    QLabel *m_responseText;
    QTableWidget *m_decodedTable;
    QTableWidget *m_log;

    QHash<QString, Exchange> m_exchanges;     // by keyOfMessage()
    QHash<quint16, QByteArray> m_lastRecords; // DID -> data record last read or written
};

#endif // DIAGNOSTICCONSOLE_H
