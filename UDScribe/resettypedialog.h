#ifndef RESETTYPEDIALOG_H
#define RESETTYPEDIALOG_H

#include "usccore/resettype.h"
#include "usccore/securitylevel.h"
#include "usccore/session.h"

#include <QDialog>
#include <QStringList>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QSpinBox;

using namespace usc;

// Asks for all properties of a reset type.
class ResetTypeDialog : public QDialog
{
    Q_OBJECT

public:
    // existingNames: names already taken by other reset types; OK is refused for these.
    // usedResetTypes: resetType sub-functions of the other reset types; they are not offered.
    // sessions, securityLevels: all of them, offered as the allowed sessions and levels.
    ResetTypeDialog(const ResetType &resetType, const QStringList &existingNames,
                    const QVector<quint8> &usedResetTypes, const QVector<Session> &sessions,
                    const QVector<SecurityLevel> &securityLevels, QWidget *parent = nullptr);

    ResetType resetType() const;

public slots:
    void accept() override;

private:
    // The power down time only exists for EnableRapidPowerShutDown.
    void updatePowerDownTime();

    ResetType m_resetType; // the reset type as it was when the dialog opened
    QStringList m_existingNames;

    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_resetTypeBox = nullptr;
    QListWidget *m_sessionList = nullptr;       // one checkable row per session; data = Session::id
    QListWidget *m_securityLevelList = nullptr; // one checkable row per level; data = SecurityLevel::id
    QSpinBox *m_powerDownTimeBox = nullptr;
    QCheckBox *m_powerDownTimeNotAvailableCheck = nullptr;
    QPlainTextEdit *m_commentEdit = nullptr;
};

#endif // RESETTYPEDIALOG_H
