#include "valuetablepropertieswidget.h"
#include "propertytable.h"
#include "usccore/database.h"

#include <QLabel>
#include <QVBoxLayout>

using namespace usc;

ValueTablePropertiesWidget::ValueTablePropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    m_propertiesTable = createReadOnlyTable({"Property", "Value"});
    m_entriesTable = createReadOnlyTable({"Value", "Description"});
    m_dataTypesTable = createReadOnlyTable({"Data Type"});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_propertiesTable);
    layout->addWidget(new QLabel("Values:"));
    layout->addWidget(m_entriesTable);
    layout->addWidget(new QLabel("Used by these data types:"));
    layout->addWidget(m_dataTypesTable);
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
    for (const QString &name : database.dataTypesUsingValueTable(valueTable.id))
        users.append({name});
    setTableRows(m_dataTypesTable, users);
}
