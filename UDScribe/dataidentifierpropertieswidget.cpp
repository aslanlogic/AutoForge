#include "dataidentifierpropertieswidget.h"
#include "dataidentifierlayoutwidget.h"
#include "propertytable.h"
#include "usccore/database.h"

#include <QLabel>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace usc;

DataIdentifierPropertiesWidget::DataIdentifierPropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    m_propertiesTable = createReadOnlyTable({"Property", "Value"});
    m_dataTypesTable = createReadOnlyTable({"Name", "Data Type", "Start Bit", "Length [Bit]", "Byte Order", "Value Type"});

    // Same grid as the DID dialog's Layout tab, but it cannot be dragged here.
    m_layoutWidget = new DataIdentifierLayoutWidget;
    m_layoutWidget->setReadOnly(true);
    auto *layoutScrollArea = new QScrollArea;
    layoutScrollArea->setWidget(m_layoutWidget);

    // Below the properties: the data type list on the left, the layout on the right,
    // with a splitter between them so the user can change how wide each is.
    auto *dataTypesColumn = new QWidget;
    auto *dataTypesColumnLayout = new QVBoxLayout(dataTypesColumn);
    dataTypesColumnLayout->setContentsMargins(0, 0, 0, 0);
    dataTypesColumnLayout->addWidget(new QLabel("Data types in this DID:"));
    dataTypesColumnLayout->addWidget(m_dataTypesTable);

    auto *layoutColumn = new QWidget;
    auto *layoutColumnLayout = new QVBoxLayout(layoutColumn);
    layoutColumnLayout->setContentsMargins(0, 0, 0, 0);
    layoutColumnLayout->addWidget(new QLabel("Layout:"));
    layoutColumnLayout->addWidget(layoutScrollArea);

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(dataTypesColumn);
    splitter->addWidget(layoutColumn);
    splitter->setChildrenCollapsible(false);
    // When the window is resized, the list takes the extra width; the layout keeps
    // its natural width (the whole grid) until the user drags the splitter.
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_propertiesTable);
    layout->addWidget(splitter);
}

void DataIdentifierPropertiesWidget::showDataIdentifier(const DataIdentifier &dataIdentifier, const Database &database)
{
    QStringList sessions;
    for (int sessionId : dataIdentifier.readSessionIds) {
        if (const Session *session = database.findSession(sessionId))
            sessions << subFunctionToText(session->sessionType) + ' ' + session->name;
    }
    QStringList securityLevels;
    for (int securityLevelId : dataIdentifier.readSecurityLevelIds) {
        if (const SecurityLevel *securityLevel = database.findSecurityLevel(securityLevelId))
            securityLevels << subFunctionToText(securityLevel->requestSeed) + ' ' + securityLevel->name;
    }

    setPropertyRows(m_propertiesTable, {
        {"Name", dataIdentifier.name},
        {"DID", didToText(dataIdentifier.did)},
        {"Length", QString("%1 bytes").arg(dataIdentifier.length)},
        {"Read in Sessions", sessions.join(", ")},
        {"Read Security Levels", securityLevels.isEmpty() ? QString("(none needed)")
                                                          : securityLevels.join(", ") + "  (any of them)"},
        {"Comment", dataIdentifier.comment},
    });

    m_dataTypesTable->setRowCount(dataIdentifier.mappings.size());
    for (int row = 0; row < dataIdentifier.mappings.size(); ++row) {
        const DataTypeMapping &mapping = dataIdentifier.mappings.at(row);
        const DataType *dataType = database.findDataType(mapping.dataTypeId);
        if (!dataType)
            continue;
        const QStringList cells = {mapping.name, dataType->name, QString::number(mapping.startBit),
                                   QString::number(dataType->length), toString(dataType->byteOrder),
                                   toString(dataType->valueType)};
        for (int column = 0; column < cells.size(); ++column) {
            auto *item = new QTableWidgetItem(cells.at(column));
            // Same colours as in the layout grid.
            item->setBackground(DataIdentifierLayoutWidget::colorFor(row));
            item->setForeground(Qt::black);
            m_dataTypesTable->setItem(row, column, item);
        }
    }

    m_layoutWidget->setLayoutData(dataIdentifier.length, dataIdentifier.mappings, database.dataTypesById());
}
