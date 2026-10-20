#include "valuetabledialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {

enum Column { ValueColumn, DescriptionColumn };

// Accepts decimal ("12", "-3") or hex with 0x in front ("0x1F").
bool textToValue(const QString &text, qint64 *value)
{
    const QString trimmed = text.trimmed();
    bool ok = false;
    if (trimmed.startsWith("0x", Qt::CaseInsensitive))
        *value = trimmed.mid(2).toLongLong(&ok, 16);
    else
        *value = trimmed.toLongLong(&ok, 10);
    return ok;
}

} // namespace

ValueTableDialog::ValueTableDialog(const ValueTable &valueTable, const QStringList &existingNames,
                                   QWidget *parent)
    : QDialog(parent)
    , m_valueTable(valueTable)
    , m_existingNames(existingNames)
{
    setWindowTitle(QString("Value Table '%1'").arg(valueTable.name));

    m_nameEdit = new QLineEdit(valueTable.name);

    // Editable directly: double-click (or just type in) a cell.
    m_entriesTable = new QTableWidget(0, 2);
    m_entriesTable->setHorizontalHeaderLabels({"Value", "Description"});
    m_entriesTable->horizontalHeader()->setStretchLastSection(true);
    m_entriesTable->verticalHeader()->setVisible(false);
    m_entriesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    for (const ValueTable::Entry &entry : valueTable.entries) {
        const int row = m_entriesTable->rowCount();
        m_entriesTable->insertRow(row);
        m_entriesTable->setItem(row, ValueColumn, new QTableWidgetItem(QString::number(entry.value)));
        m_entriesTable->setItem(row, DescriptionColumn, new QTableWidgetItem(entry.description));
    }

    auto *addButton = new QPushButton("Add");
    auto *removeButton = new QPushButton("Remove");
    connect(addButton, &QPushButton::clicked, this, &ValueTableDialog::addEntry);
    connect(removeButton, &QPushButton::clicked, this, &ValueTableDialog::removeEntries);
    auto *entryButtons = new QHBoxLayout;
    entryButtons->addWidget(addButton);
    entryButtons->addWidget(removeButton);
    entryButtons->addStretch();

    m_commentEdit = new QPlainTextEdit(valueTable.comment);
    m_commentEdit->setMaximumHeight(80);

    auto *form = new QFormLayout;
    form->addRow("Name:", m_nameEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(new QLabel("Values (raw value as sent on the bus, decimal or 0x hex):"));
    layout->addWidget(m_entriesTable);
    layout->addLayout(entryButtons);
    layout->addWidget(new QLabel("Comment:"));
    layout->addWidget(m_commentEdit);
    layout->addWidget(buttons);

    resize(480, 460);
}

ValueTable ValueTableDialog::valueTable() const
{
    // Start from the value table we were given, so fields the dialog does not show (like id) are kept.
    ValueTable valueTable = m_valueTable;
    valueTable.name = m_nameEdit->text().trimmed();
    valueTable.comment = m_commentEdit->toPlainText();

    valueTable.entries.clear();
    for (int row = 0; row < m_entriesTable->rowCount(); ++row) {
        ValueTable::Entry entry;
        const QTableWidgetItem *valueItem = m_entriesTable->item(row, ValueColumn);
        const QTableWidgetItem *descriptionItem = m_entriesTable->item(row, DescriptionColumn);
        textToValue(valueItem ? valueItem->text() : QString(), &entry.value);
        entry.description = descriptionItem ? descriptionItem->text().trimmed() : QString();
        valueTable.entries.append(entry);
    }
    std::sort(valueTable.entries.begin(), valueTable.entries.end(),
              [](const ValueTable::Entry &a, const ValueTable::Entry &b) { return a.value < b.value; });
    return valueTable;
}

void ValueTableDialog::accept()
{
    const QString name = m_nameEdit->text().trimmed();
    QString nameError;
    if (name.isEmpty())
        nameError = "Please enter a name for the value table.";
    else if (m_existingNames.contains(name))
        nameError = QString("A value table named '%1' already exists.").arg(name);
    if (!nameError.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), nameError);
        m_nameEdit->setFocus();
        m_nameEdit->selectAll();
        return;
    }

    // Every row needs a whole-number value that is not used twice, and a description.
    QSet<qint64> seen;
    for (int row = 0; row < m_entriesTable->rowCount(); ++row) {
        const QTableWidgetItem *valueItem = m_entriesTable->item(row, ValueColumn);
        const QTableWidgetItem *descriptionItem = m_entriesTable->item(row, DescriptionColumn);
        qint64 value = 0;
        if (!valueItem || !textToValue(valueItem->text(), &value)) {
            showError(row, ValueColumn, QString("Row %1: the value must be a whole number, "
                                                "decimal (12) or hex with 0x in front (0x1F).").arg(row + 1));
            return;
        }
        if (seen.contains(value)) {
            showError(row, ValueColumn, QString("Row %1: the value %2 is already in the table.").arg(row + 1).arg(value));
            return;
        }
        seen.insert(value);
        if (!descriptionItem || descriptionItem->text().trimmed().isEmpty()) {
            showError(row, DescriptionColumn, QString("Row %1: please enter a description.").arg(row + 1));
            return;
        }
    }

    QDialog::accept();
}

void ValueTableDialog::showError(int row, int column, const QString &error)
{
    QMessageBox::warning(this, windowTitle(), error);
    m_entriesTable->setCurrentCell(row, column);
    m_entriesTable->setFocus();
}

void ValueTableDialog::addEntry()
{
    // Suggest the value after the largest one so far: 0, 1, 2, ...
    qint64 next = 0;
    for (int row = 0; row < m_entriesTable->rowCount(); ++row) {
        qint64 value = 0;
        const QTableWidgetItem *item = m_entriesTable->item(row, ValueColumn);
        if (item && textToValue(item->text(), &value))
            next = qMax(next, value + 1);
    }

    const int row = m_entriesTable->rowCount();
    m_entriesTable->insertRow(row);
    m_entriesTable->setItem(row, ValueColumn, new QTableWidgetItem(QString::number(next)));
    m_entriesTable->setItem(row, DescriptionColumn, new QTableWidgetItem);
    // Start typing the description right away.
    m_entriesTable->setCurrentCell(row, DescriptionColumn);
    m_entriesTable->editItem(m_entriesTable->item(row, DescriptionColumn));
}

void ValueTableDialog::removeEntries()
{
    // Remove from the bottom up, so the row numbers of the others do not shift.
    QList<int> rows;
    for (const QModelIndex &index : m_entriesTable->selectionModel()->selectedRows())
        rows << index.row();
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows)
        m_entriesTable->removeRow(row);
}
