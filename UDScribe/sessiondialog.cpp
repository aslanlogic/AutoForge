#include "sessiondialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace usc;

SessionDialog::SessionDialog(const Session &session, const QStringList &existingNames,
                             const QVector<quint8> &usedSessionTypes, QWidget *parent)
    : QDialog(parent)
    , m_session(session)
    , m_existingNames(existingNames)
{
    setWindowTitle(QString("Session '%1'").arg(session.name));

    m_nameEdit = new QLineEdit(session.name);

    // Only sub-functions the session can have, so the choice is always allowed:
    // the default session keeps 0x01; the others get the valid ones that are
    // neither 0x01 nor used by another session.
    m_sessionTypeBox = new QComboBox;
    const bool isDefault = session.sessionType == DefaultSessionType;
    for (int value = 0; value <= 0xFF; ++value) {
        const quint8 sessionType = quint8(value);
        const bool offered = isDefault ? sessionType == DefaultSessionType
                                       : isValidSessionType(sessionType) && sessionType != DefaultSessionType
                                             && !usedSessionTypes.contains(sessionType);
        if (offered)
            m_sessionTypeBox->addItem(subFunctionToText(sessionType) + ' ' + sessionTypeDescription(sessionType), value);
    }
    m_sessionTypeBox->setCurrentIndex(m_sessionTypeBox->findData(int(session.sessionType)));
    m_sessionTypeBox->setEnabled(!isDefault);

    // The largest values the positive response can report (see session.h).
    m_p2Box = new QSpinBox;
    m_p2Box->setRange(0, 65535);
    m_p2Box->setSuffix(" ms");
    m_p2Box->setValue(session.p2);
    m_p2StarBox = new QSpinBox;
    m_p2StarBox->setRange(0, 655350);
    m_p2StarBox->setSingleStep(P2StarStep);
    m_p2StarBox->setSuffix(" ms");
    m_p2StarBox->setValue(session.p2Star);

    m_commentEdit = new QPlainTextEdit(session.comment);

    auto *form = new QFormLayout;
    form->addRow("Name:", m_nameEdit);
    form->addRow("Session Type:", m_sessionTypeBox);
    form->addRow("P2 (until the first response):", m_p2Box);
    form->addRow("P2* (after each NRC 0x78):", m_p2StarBox);
    form->addRow("Comment:", m_commentEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    resize(480, 340);
}

Session SessionDialog::session() const
{
    // Start from the session we were given, so fields the dialog does not show (like id) are kept.
    Session session = m_session;
    session.name = m_nameEdit->text().trimmed();
    session.sessionType = quint8(m_sessionTypeBox->currentData().toUInt());
    session.p2 = m_p2Box->value();
    session.p2Star = m_p2StarBox->value();
    session.comment = m_commentEdit->toPlainText();
    return session;
}

void SessionDialog::accept()
{
    const QString name = m_nameEdit->text().trimmed();

    QWidget *badField = m_nameEdit;
    QString error;
    if (name.isEmpty()) {
        error = "Please enter a name for the session.";
    } else if (m_existingNames.contains(name)) {
        error = QString("A session named '%1' already exists.").arg(name);
    } else if (m_sessionTypeBox->currentIndex() < 0) {
        error = "Every sessionType sub-function is already used by another session.";
        badField = m_sessionTypeBox;
    } else if (m_p2StarBox->value() % P2StarStep != 0) {
        error = QString("P2* must be a multiple of %1 ms: the ECU reports it in %1 ms steps.").arg(P2StarStep);
        badField = m_p2StarBox;
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
