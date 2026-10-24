#ifndef ACCESSDIALOG_H
#define ACCESSDIALOG_H

#include "usccore/securitylevel.h"
#include "usccore/session.h"

#include <QDialog>
#include <QVector>

class QListWidget;

using namespace usc;

// Asks in which sessions something may be done (at least one) and which security
// levels allow it (any of them; none = no security needed). Used for reading a DID
// with ReadDataByIdentifier (0x22) and writing it with WriteDataByIdentifier (0x2E).
class AccessDialog : public QDialog
{
    Q_OBJECT

public:
    // title: the window title; what: "read" or "written", used in the labels.
    // sessionIds, securityLevelIds: what is checked when the dialog opens.
    AccessDialog(const QString &title, const QString &what, const QVector<Session> &sessions,
                 const QVector<SecurityLevel> &securityLevels, const QVector<int> &sessionIds,
                 const QVector<int> &securityLevelIds, QWidget *parent = nullptr);

    QVector<int> sessionIds() const;       // Session::id, in the order of the list
    QVector<int> securityLevelIds() const; // SecurityLevel::id, in the order of the list

public slots:
    void accept() override;

private:
    QListWidget *m_sessionList = nullptr;       // one checkable row per session; data = Session::id
    QListWidget *m_securityLevelList = nullptr; // one checkable row per level; data = SecurityLevel::id
};

#endif // ACCESSDIALOG_H
