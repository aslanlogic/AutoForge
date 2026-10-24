#include "datatypepropertieswidget.h"
#include "propertytable.h"
#include "usccore/database.h"

#include <QLabel>
#include <QLocale>
#include <QVBoxLayout>

using namespace usc;

DataTypePropertiesWidget::DataTypePropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    m_propertiesTable = createReadOnlyTable({"Property", "Value"});
    m_dataIdentifiersTable = createReadOnlyTable({"DID", "Name"});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_propertiesTable);
    layout->addWidget(new QLabel("Used by these DIDs:"));
    layout->addWidget(m_dataIdentifiersTable);
}

namespace {

QString valueTableName(const DataType &dataType, const Database &database)
{
    const ValueTable *valueTable = database.findValueTable(dataType.valueTableId);
    return valueTable ? valueTable->name : QString("<none>");
}

} // namespace

void DataTypePropertiesWidget::showDataType(const DataType &dataType, const Database &database)
{
    auto number = [](double value) {
        return QLocale::c().toString(value, 'g', QLocale::FloatingPointShortest);
    };

    setPropertyRows(m_propertiesTable, {
        {"Name", dataType.name},
        {"Length [Bit]", QString::number(dataType.length)},
        {"Byte Order", toString(dataType.byteOrder)},
        {"Value Type", toString(dataType.valueType)},
        {"Unit", dataType.unit},
        {"Factor", number(dataType.factor)},
        {"Offset", number(dataType.offset)},
        {"Minimum", number(dataType.minimum)},
        {"Maximum", number(dataType.maximum)},
        {"Value Table", valueTableName(dataType, database)},
        {"Comment", dataType.comment},
    });

    QVector<QStringList> users;
    for (const DataIdentifier &dataIdentifier : database.allDataIdentifiers()) {
        for (const DataTypeMapping &mapping : dataIdentifier.mappings) {
            if (mapping.dataTypeId == dataType.id) {
                users.append({didToText(dataIdentifier.did), dataIdentifier.name});
                break;
            }
        }
    }
    setTableRows(m_dataIdentifiersTable, users);
}
