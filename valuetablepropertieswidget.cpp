#include "valuetablepropertieswidget.h"
#include "database.h"
#include "propertytable.h"

#include <QLabel>
#include <QVBoxLayout>

ValueTablePropertiesWidget::ValueTablePropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    m_propertiesTable = createReadOnlyTable({"Property", "Value"});
    m_entriesTable = createReadOnlyTable({"Value", "Description"});
    m_signalsTable = createReadOnlyTable({"Signal"});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_propertiesTable);
    layout->addWidget(new QLabel("Values:"));
    layout->addWidget(m_entriesTable);
    layout->addWidget(new QLabel("Used by these signals:"));
    layout->addWidget(m_signalsTable);
}

void ValueTablePropertiesWidget::showValueTable(const ValueTable &valueTable, const Database &database)
{
    setPropertyRows(m_propertiesTable, {
        {"Name", valueTable.name},
        {"Comment", valueTable.comment},
    });

    QVector<QStringList> entries;
    for (const ValueTable::Entry &entry : valueTable.entries)
        entries.append({QString::number(entry.value), entry.description});
    setTableRows(m_entriesTable, entries);

    QVector<QStringList> users;
    for (const QString &name : database.signalsUsingValueTable(valueTable.id))
        users.append({name});
    setTableRows(m_signalsTable, users);
}
