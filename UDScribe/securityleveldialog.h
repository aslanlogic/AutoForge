#ifndef SECURITYLEVELDIALOG_H
#define SECURITYLEVELDIALOG_H

#include "usccore/securitylevel.h"
#include "usccore/session.h"

#include <QDialog>
#include <QStringList>
#include <QVector>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QSpinBox;

using namespace usc;

// Asks for all properties of a security level.
class SecurityLevelDialog : public QDialog
{
    Q_OBJECT

public:
    // existingNames: names already taken by other security levels; OK is refused for these.
    // usedRequestSeeds: requestSeed sub-functions of the other security levels; they are not offered.
    // sessions: all sessions, offered as the sessions the level is allowed in.
    SecurityLevelDialog(const SecurityLevel &securityLevel, const QStringList &existingNames,
                        const QVector<quint8> &usedRequestSeeds, const QVector<Session> &sessions,
                        QWidget *parent = nullptr);

    SecurityLevel securityLevel() const;

public slots:
    void accept() override;

private:
    void updateSendKey();

    SecurityLevel m_securityLevel; // the security level as it was when the dialog opened
    QStringList m_existingNames;

    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_requestSeedBox = nullptr;
    QLabel *m_sendKeyLabel = nullptr;
    QSpinBox *m_seedLengthBox = nullptr;
    QSpinBox *m_keyLengthBox = nullptr;
    QSpinBox *m_maxAttemptsBox = nullptr;
    QListWidget *m_sessionList = nullptr; // one checkable row per session; data = Session::id
    QPlainTextEdit *m_commentEdit = nullptr;
};

#endif // SECURITYLEVELDIALOG_H
