#include "networkpropertieswidget.h"
#include "database.h"
#include "propertytable.h"

#include <QVBoxLayout>

NetworkPropertiesWidget::NetworkPropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    m_propertiesTable = createReadOnlyTable({"Property", "Value"});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_propertiesTable);
    layout->addStretch();
}

void NetworkPropertiesWidget::showNetwork(const Database &database, const QString &filePath)
{
    setPropertyRows(m_propertiesTable, {
        {"File", filePath.isEmpty() ? "(not saved yet)" : filePath},
        {"Network Nodes", QString::number(database.allNodes().size())},
        {"Messages", QString::number(database.allMessages().size())},
        {"Signals", QString::number(database.allSignals().size())},
    });
}
