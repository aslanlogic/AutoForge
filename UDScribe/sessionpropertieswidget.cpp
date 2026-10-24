#include "sessionpropertieswidget.h"
#include "propertytable.h"
#include "usccore/session.h"

#include <QVBoxLayout>

using namespace usc;

SessionPropertiesWidget::SessionPropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    m_propertiesTable = createReadOnlyTable({"Property", "Value"});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_propertiesTable);
    layout->addStretch(); // the table is only as tall as its rows
}

void SessionPropertiesWidget::showSession(const Session &session)
{
    setPropertyRows(m_propertiesTable, {
        {"Name", session.name},
        {"Session Type", subFunctionToText(session.sessionType)},
        {"P2", QString("%1 ms  (until the first response)").arg(session.p2)},
        {"P2*", QString("%1 ms  (after each NRC 0x78, response pending)").arg(session.p2Star)},
        {"Comment", session.comment},
    });
}
