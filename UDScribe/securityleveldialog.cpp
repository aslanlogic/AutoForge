#include "securityleveldialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace usc;

SecurityLevelDialog::SecurityLevelDialog(const SecurityLevel &securityLevel, const QStringList &existingNames,
                                         const QVector<quint8> &usedRequestSeeds, QWidget *parent)
    : QDialog(parent)
    , m_securityLevel(securityLevel)
    , m_existingNames(existingNames)
{
    setWindowTitle(QString("Security Level '%1'").arg(securityLevel.name));

    m_nameEdit = new QLineEdit(securityLevel.name);

    // Only valid sub-functions that no other security level uses, so the
    // choice is always allowed.
    m_requestSeedBox = new QComboBox;
    for (int value = 0; value <= 0xFF; ++value) {
        const quint8 requestSeed = quint8(value);
        if (isValidRequestSeed(requestSeed) && !usedRequestSeeds.contains(requestSeed))
            m_requestSeedBox->addItem(subFunctionToText(requestSeed), value);
    }
    m_requestSeedBox->setCurrentIndex(m_requestSeedBox->findData(int(securityLevel.requestSeed)));
    m_sendKeyLabel = new QLabel;
    connect(m_requestSeedBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SecurityLevelDialog::updateSendKey);
    updateSendKey();

    // A seed or key must fit in one ISO-TP message (4095 bytes) after the SID and sub-function.
    m_seedLengthBox = new QSpinBox;
    m_seedLengthBox->setRange(1, 4093);
    m_seedLengthBox->setSuffix(" bytes");
    m_seedLengthBox->setValue(securityLevel.seedLength);
    m_keyLengthBox = new QSpinBox;
    m_keyLengthBox->setRange(1, 4093);
    m_keyLengthBox->setSuffix(" bytes");
    m_keyLengthBox->setValue(securityLevel.keyLength);
    m_maxAttemptsBox = new QSpinBox;
    m_maxAttemptsBox->setRange(0, 255);
    m_maxAttemptsBox->setValue(securityLevel.maxAttempts);

    m_commentEdit = new QPlainTextEdit(securityLevel.comment);

    auto *form = new QFormLayout;
    form->addRow("Name:", m_nameEdit);
    form->addRow("Request Seed:", m_requestSeedBox);
    form->addRow("Send Key:", m_sendKeyLabel);
    form->addRow("Seed Length:", m_seedLengthBox);
    form->addRow("Key Length:", m_keyLengthBox);
    form->addRow("Max Attempts:", m_maxAttemptsBox);
    form->addRow("Comment:", m_commentEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    resize(480, 400);
}

SecurityLevel SecurityLevelDialog::securityLevel() const
{
    // Start from the security level we were given, so fields the dialog does not show (like id) are kept.
    SecurityLevel securityLevel = m_securityLevel;
    securityLevel.name = m_nameEdit->text().trimmed();
    securityLevel.requestSeed = quint8(m_requestSeedBox->currentData().toUInt());
    securityLevel.seedLength = m_seedLengthBox->value();
    securityLevel.keyLength = m_keyLengthBox->value();
    securityLevel.maxAttempts = m_maxAttemptsBox->value();
    securityLevel.comment = m_commentEdit->toPlainText();
    return securityLevel;
}

void SecurityLevelDialog::accept()
{
    const QString name = m_nameEdit->text().trimmed();

    QWidget *badField = m_nameEdit;
    QString error;
    if (name.isEmpty()) {
        error = "Please enter a name for the security level.";
    } else if (m_existingNames.contains(name)) {
        error = QString("A security level named '%1' already exists.").arg(name);
    } else if (m_requestSeedBox->currentIndex() < 0) {
        error = "Every requestSeed sub-function is already used by another security level.";
        badField = m_requestSeedBox;
    }

    if (!error.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), error);
        badField->setFocus();
        if (badField == m_nameEdit)
            m_nameEdit->selectAll();
        return;
    }

    QDialog::accept();
}

void SecurityLevelDialog::updateSendKey()
{
    const QVariant requestSeed = m_requestSeedBox->currentData();
    m_sendKeyLabel->setText(requestSeed.isValid() ? subFunctionToText(sendKey(quint8(requestSeed.toUInt())))
                                                  : QString());
}
