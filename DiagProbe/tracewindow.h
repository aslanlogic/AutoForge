#ifndef TRACEWINDOW_H
#define TRACEWINDOW_H

#include "ffcore/database.h"
#include "usccore/database.h"

#include <QHash>
#include <QWidget>

class QCheckBox;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class Tester;

// Every message on the bus, in two views:
//  - By message: one row per message ID and direction, updated in place (data,
//    cycle time, count); expand a row to see its signals decoded with the .ffxml,
//    each with a Graph button. The UDS request and response IDs show as
//    DiagRequest / DiagResponse with the last UDS message explained.
//  - Chronological: every message in the order it was seen.
class TraceWindow : public QWidget
{
    Q_OBJECT

public:
    explicit TraceWindow(Tester *tester, QWidget *parent = nullptr);

    // Either may be nullptr. Clears the trace.
    void setDatabases(const usc::Database *description, const ff::Database *database);
    void clear();

signals:
    // The Graph button of a signal was clicked (ff Message::id and Signal::id).
    void graphRequested(int messageId, int signalId);

private:
    struct MessageRow
    {
        QTreeWidgetItem *item = nullptr;
        quint32 id = 0;
        bool transmitted = false;
        double lastTime = -1.0;
        double cycle = 0.0; // s between the last two
        quint64 count = 0;
        QByteArray data;
        bool dirty = false;
    };
    struct Frame
    {
        double time;
        quint32 id;
        QByteArray data;
        bool transmitted;
    };

    void onFrame(double time, quint32 id, const QByteArray &data, bool transmitted);
    void flush();
    MessageRow &rowFor(quint32 id, bool transmitted);
    void updateRow(MessageRow &row);
    QString nameOf(quint32 id) const;
    QString interpretation(quint32 id, const QByteArray &data) const;
    bool isRequestId(quint32 id) const;
    bool isResponseId(quint32 id) const;

    Tester *m_tester;
    const usc::Database *m_description = nullptr;
    const ff::Database *m_database = nullptr;
    QTreeWidget *m_byMessage;
    QTableWidget *m_chronological;
    QCheckBox *m_pause;
    QHash<quint64, MessageRow> m_rows; // by (transmitted << 32 | id)
    QVector<Frame> m_pending;          // for the chronological view
};

#endif // TRACEWINDOW_H
