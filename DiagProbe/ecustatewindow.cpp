#include "ecustatewindow.h"
#include "diagdefaults.h"
#include "tester.h"

#include <QFormLayout>
#include <QLabel>
#include <QTimer>

EcuStateWindow::EcuStateWindow(Tester *tester, QWidget *parent)
    : QWidget(parent)
    , m_tester(tester)
    , m_measurement(new QLabel)
    , m_session(new QLabel)
    , m_security(new QLabel)
    , m_s3(new QLabel)
    , m_lastResponse(new QLabel)
{
    m_lastResponse->setWordWrap(true);
    QFont bold = m_session->font();
    bold.setBold(true);
    m_session->setFont(bold);
    m_security->setFont(bold);

    auto *layout = new QFormLayout(this);
    layout->addRow("Measurement:", m_measurement);
    layout->addRow("Session:", m_session);
    layout->addRow("Security:", m_security);
    layout->addRow("S3:", m_s3);
    layout->addRow("Last response:", m_lastResponse);

    connect(m_tester, &Tester::stateChanged, this, &EcuStateWindow::refresh);
    auto *timer = new QTimer(this); // for the S3 countdown
    connect(timer, &QTimer::timeout, this, &EcuStateWindow::refresh);
    timer->start(200);
    refresh();
}

void EcuStateWindow::refresh()
{
    if (!m_tester->isRunning()) {
        m_measurement->setText("Stopped");
        m_session->setText("—");
        m_security->setText("—");
        m_s3->setText("—");
        m_lastResponse->setText(m_tester->lastResponse().isEmpty() ? "—" : m_tester->lastResponse());
        return;
    }
    m_measurement->setText("Running");

    const usc::Database *description = m_tester->description();
    const usc::Session *session = description ? description->findSession(m_tester->sessionId()) : nullptr;
    m_session->setText(session ? QString("%1 (%2)").arg(session->name, usc::subFunctionToText(session->sessionType))
                               : QString("—"));

    QStringList levels;
    if (description) {
        for (const usc::SecurityLevel &level : description->allSecurityLevels()) {
            if (m_tester->unlockedLevelIds().contains(level.id))
                levels << level.name;
        }
    }
    m_security->setText(levels.isEmpty() ? "Locked" : "Unlocked: " + levels.join(", "));
    m_security->setStyleSheet(levels.isEmpty() ? QString() : "color:#1d5e1d;");

    const double remaining = m_tester->s3Remaining();
    if (remaining < 0)
        m_s3->setText("Default session, no timeout");
    else if (m_tester->isTesterPresentEnabled())
        m_s3->setText(QString("Kept alive by TesterPresent every %1 s").arg(TesterPresentInterval / 1000.0));
    else
        m_s3->setText(QString("Back to Default in %1 s").arg(remaining, 0, 'f', 1));

    m_lastResponse->setText(m_tester->lastResponse().isEmpty() ? "—" : m_tester->lastResponse());
}
