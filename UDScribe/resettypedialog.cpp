#include "resettypedialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace usc;

namespace {

// The ids of the checked rows of a list made by ResetTypeDialog.
QVector<int> checkedIds(const QListWidget *list)
{
    QVector<int> ids;
    for (int row = 0; row < list->count(); ++row) {
        const QListWidgetItem *item = list->item(row);
        if (item->checkState() == Qt::Checked)
            ids.append(item->data(Qt::UserRole).toInt());
    }
    return ids;
}

void addCheckableRow(QListWidget *list, const QString &text, int id, bool checked)
{
    auto *item = new QListWidgetItem(text, list);
    item->setData(Qt::UserRole, id);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}

} // namespace

ResetTypeDialog::ResetTypeDialog(const ResetType &resetType, const QStringList &existingNames,
                                 const QVector<quint8> &usedResetTypes, const QVector<Session> &sessions,
                                 const QVector<SecurityLevel> &securityLevels, QWidget *parent)
    : QDialog(parent)
    , m_resetType(resetType)
    , m_existingNames(existingNames)
{
    setWindowTitle(QString("Reset Type '%1'").arg(resetType.name));

    m_nameEdit = new QLineEdit(resetType.name);

    // Only valid sub-functions that no other reset type uses, so the choice is always allowed.
    m_resetTypeBox = new QComboBox;
    for (int value = 0; value <= 0xFF; ++value) {
        const quint8 type = quint8(value);
        if (isValidResetType(type) && !usedResetTypes.contains(type))
            m_resetTypeBox->addItem(subFunctionToText(type) + ' ' + resetTypeDescription(type), value);
    }
    m_resetTypeBox->setCurrentIndex(m_resetTypeBox->findData(int(resetType.resetType)));
    connect(m_resetTypeBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ResetTypeDialog::updatePowerDownTime);

    m_sessionList = new QListWidget;
    for (const Session &session : sessions) {
        addCheckableRow(m_sessionList, subFunctionToText(session.sessionType) + ' ' + session.name, session.id,
                        resetType.sessionIds.contains(session.id));
    }
    m_securityLevelList = new QListWidget;
    for (const SecurityLevel &securityLevel : securityLevels) {
        addCheckableRow(m_securityLevelList, subFunctionToText(securityLevel.requestSeed) + ' ' + securityLevel.name,
                        securityLevel.id, resetType.securityLevelIds.contains(securityLevel.id));
    }

    // 0-254 s; "not available" is sent as 0xFF.
    m_powerDownTimeBox = new QSpinBox;
    m_powerDownTimeBox->setRange(0, PowerDownTimeNotAvailable - 1);
    m_powerDownTimeBox->setSuffix(" s");
    m_powerDownTimeNotAvailableCheck = new QCheckBox("not available");
    const bool notAvailable = resetType.powerDownTime == PowerDownTimeNotAvailable;
    m_powerDownTimeBox->setValue(notAvailable ? 0 : resetType.powerDownTime);
    m_powerDownTimeNotAvailableCheck->setChecked(notAvailable);
    connect(m_powerDownTimeNotAvailableCheck, &QCheckBox::toggled, this, &ResetTypeDialog::updatePowerDownTime);
    auto *powerDownTimeRow = new QHBoxLayout;
    powerDownTimeRow->addWidget(m_powerDownTimeBox, 1);
    powerDownTimeRow->addWidget(m_powerDownTimeNotAvailableCheck);
    updatePowerDownTime();

    m_commentEdit = new QPlainTextEdit(resetType.comment);

    auto *form = new QFormLayout;
    form->addRow("Name:", m_nameEdit);
    form->addRow("Reset Type:", m_resetTypeBox);
    form->addRow("Allowed in Sessions:", m_sessionList);
    form->addRow("Security Levels\n(any of them;\nnone = no security):", m_securityLevelList);
    form->addRow("Power Down Time:", powerDownTimeRow);
    form->addRow("Comment:", m_commentEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    resize(520, 600);
}

ResetType ResetTypeDialog::resetType() const
{
    // Start from the reset type we were given, so fields the dialog does not show (like id) are kept.
    ResetType resetType = m_resetType;
    resetType.name = m_nameEdit->text().trimmed();
    resetType.resetType = quint8(m_resetTypeBox->currentData().toUInt());
    resetType.sessionIds = checkedIds(m_sessionList);
    resetType.securityLevelIds = checkedIds(m_securityLevelList);
    // Only EnableRapidPowerShutDown has a power down time; the others keep "not available".
    if (resetType.resetType == EnableRapidPowerShutDown && !m_powerDownTimeNotAvailableCheck->isChecked())
        resetType.powerDownTime = m_powerDownTimeBox->value();
    else
        resetType.powerDownTime = PowerDownTimeNotAvailable;
    resetType.comment = m_commentEdit->toPlainText();
    return resetType;
}

void ResetTypeDialog::accept()
{
    const QString name = m_nameEdit->text().trimmed();

    QWidget *badField = m_nameEdit;
    QString error;
    if (name.isEmpty()) {
        error = "Please enter a name for the reset type.";
    } else if (m_existingNames.contains(name)) {
        error = QString("A reset type named '%1' already exists.").arg(name);
    } else if (m_resetTypeBox->currentIndex() < 0) {
        error = "Every resetType sub-function is already used by another reset type.";
        badField = m_resetTypeBox;
    } else if (checkedIds(m_sessionList).isEmpty()) {
        error = "Please choose at least one session the reset is allowed in.";
        badField = m_sessionList;
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

void ResetTypeDialog::updatePowerDownTime()
{
    const bool used = m_resetTypeBox->currentData().toUInt() == EnableRapidPowerShutDown;
    m_powerDownTimeNotAvailableCheck->setEnabled(used);
    m_powerDownTimeBox->setEnabled(used && !m_powerDownTimeNotAvailableCheck->isChecked());
}
