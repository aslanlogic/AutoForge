#include "resettypepropertieswidget.h"
#include "propertytable.h"
#include "usccore/database.h"

#include <QVBoxLayout>

using namespace usc;

ResetTypePropertiesWidget::ResetTypePropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    m_propertiesTable = createReadOnlyTable({"Property", "Value"});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_propertiesTable);
    layout->addStretch(); // the table is only as tall as its rows
}

void ResetTypePropertiesWidget::showResetType(const ResetType &resetType, const Database &database)
{
    QStringList sessions;
    for (int sessionId : resetType.sessionIds) {
        if (const Session *session = database.findSession(sessionId))
            sessions << subFunctionToText(session->sessionType) + ' ' + session->name;
    }
    QStringList securityLevels;
    for (int securityLevelId : resetType.securityLevelIds) {
        if (const SecurityLevel *securityLevel = database.findSecurityLevel(securityLevelId))
            securityLevels << subFunctionToText(securityLevel->requestSeed) + ' ' + securityLevel->name;
    }

    QVector<QPair<QString, QString>> rows = {
        {"Name", resetType.name},
        {"Reset Type", subFunctionToText(resetType.resetType) + ' ' + resetTypeDescription(resetType.resetType)},
        {"Allowed in Sessions", sessions.join(", ")},
        {"Security Levels", securityLevels.isEmpty() ? QString("(none needed)")
                                                     : securityLevels.join(", ") + "  (any of them)"},
    };
    if (resetType.resetType == EnableRapidPowerShutDown) {
        rows.append({"Power Down Time", resetType.powerDownTime == PowerDownTimeNotAvailable
                                            ? QString("not available")
                                            : QString("%1 s").arg(resetType.powerDownTime)});
    }
    rows.append({"Comment", resetType.comment});
    setPropertyRows(m_propertiesTable, rows);
}
