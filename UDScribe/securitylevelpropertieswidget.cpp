#include "securitylevelpropertieswidget.h"
#include "propertytable.h"
#include "usccore/database.h"

#include <QVBoxLayout>

using namespace usc;

SecurityLevelPropertiesWidget::SecurityLevelPropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    m_propertiesTable = createReadOnlyTable({"Property", "Value"});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_propertiesTable);
    layout->addStretch(); // the table is only as tall as its rows
}

void SecurityLevelPropertiesWidget::showSecurityLevel(const SecurityLevel &securityLevel, const Database &database)
{
    QStringList sessions;
    for (int sessionId : securityLevel.sessionIds) {
        if (const Session *session = database.findSession(sessionId))
            sessions << subFunctionToText(session->sessionType) + ' ' + session->name;
    }

    setPropertyRows(m_propertiesTable, {
        {"Name", securityLevel.name},
        {"Request Seed", subFunctionToText(securityLevel.requestSeed)},
        {"Send Key", subFunctionToText(sendKey(securityLevel.requestSeed))},
        {"Seed Length", QString("%1 bytes").arg(securityLevel.seedLength)},
        {"Key Length", QString("%1 bytes").arg(securityLevel.keyLength)},
        {"Max Attempts", QString::number(securityLevel.maxAttempts)},
        {"Allowed in Sessions", sessions.join(", ")},
        {"Comment", securityLevel.comment},
    });
}
