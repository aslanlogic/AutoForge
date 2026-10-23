#ifndef SECURITYLEVELDIALOG_H
#define SECURITYLEVELDIALOG_H

#include "usccore/securitylevel.h"

#include <QDialog>
#include <QStringList>
#include <QVector>

class QComboBox;
class QLabel;
class QLineEdit;
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
    SecurityLevelDialog(const SecurityLevel &securityLevel, const QStringList &existingNames,
                        const QVector<quint8> &usedRequestSeeds, QWidget *parent = nullptr);

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
    QPlainTextEdit *m_commentEdit = nullptr;
};

#endif // SECURITYLEVELDIALOG_H
