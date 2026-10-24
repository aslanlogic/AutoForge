#include "accessdialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QVBoxLayout>

using namespace usc;

namespace {

// The ids of the checked rows of a list made by addCheckableRow().
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

AccessDialog::AccessDialog(const QString &title, const QString &what, const QVector<Session> &sessions,
                           const QVector<SecurityLevel> &securityLevels, const QVector<int> &sessionIds,
                           const QVector<int> &securityLevelIds, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);

    m_sessionList = new QListWidget;
    for (const Session &session : sessions) {
        addCheckableRow(m_sessionList, subFunctionToText(session.sessionType) + ' ' + session.name, session.id,
                        sessionIds.contains(session.id));
    }
    m_securityLevelList = new QListWidget;
    for (const SecurityLevel &securityLevel : securityLevels) {
        addCheckableRow(m_securityLevelList, subFunctionToText(securityLevel.requestSeed) + ' ' + securityLevel.name,
                        securityLevel.id, securityLevelIds.contains(securityLevel.id));
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QString("Sessions it can be %1 in (at least one):").arg(what)));
    layout->addWidget(m_sessionList);
    layout->addWidget(new QLabel(QString("Security levels that allow it to be %1 (any of them; "
                                         "none = no security needed):").arg(what)));
    layout->addWidget(m_securityLevelList);
    layout->addWidget(buttons);

    resize(480, 460);
}

QVector<int> AccessDialog::sessionIds() const
{
    return checkedIds(m_sessionList);
}

QVector<int> AccessDialog::securityLevelIds() const
{
    return checkedIds(m_securityLevelList);
}

void AccessDialog::accept()
{
    if (sessionIds().isEmpty()) {
        QMessageBox::warning(this, windowTitle(), "Please choose at least one session.");
        m_sessionList->setFocus();
        return;
    }
    QDialog::accept();
}
