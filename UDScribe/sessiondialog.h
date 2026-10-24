#ifndef SESSIONDIALOG_H
#define SESSIONDIALOG_H

#include "usccore/session.h"

#include <QDialog>
#include <QStringList>
#include <QVector>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

using namespace usc;

// Asks for all properties of a session. The default session keeps its
// sessionType (0x01); other sessions cannot become the default session.
class SessionDialog : public QDialog
{
    Q_OBJECT

public:
    // existingNames: names already taken by other sessions; OK is refused for these.
    // usedSessionTypes: sessionType sub-functions of the other sessions; they are not offered.
    SessionDialog(const Session &session, const QStringList &existingNames,
                  const QVector<quint8> &usedSessionTypes, QWidget *parent = nullptr);

    Session session() const;

public slots:
    void accept() override;

private:
    Session m_session; // the session as it was when the dialog opened
    QStringList m_existingNames;

    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_sessionTypeBox = nullptr;
    QSpinBox *m_p2Box = nullptr;
    QSpinBox *m_p2StarBox = nullptr;
    QPlainTextEdit *m_commentEdit = nullptr;
};

#endif // SESSIONDIALOG_H
