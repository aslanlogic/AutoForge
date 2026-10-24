#include "mainwindow.h"
#include "diagnosticconsole.h"
#include "ecustatewindow.h"
#include "generatorwindow.h"
#include "graphwindow.h"
#include "tester.h"
#include "tracewindow.h"
#include "ffcore/database.h"
#include "ffcore/xmlstorage.h"
#include "usccore/database.h"
#include "usccore/xmlstorage.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1400, 900);

    m_diagnosticDescription = new usc::Database(this);
    m_messageDatabase = new ff::Database(this);
    m_tester = new Tester(this);

    m_mdiArea = new QMdiArea;
    m_mdiArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_mdiArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setCentralWidget(m_mdiArea);

    setupWindows();
    setupMenus();
    setupToolBar();

    m_descriptionLabel = new QLabel;
    m_databaseLabel = new QLabel;
    m_timeLabel = new QLabel;
    m_loadLabel = new QLabel;
    m_countLabel = new QLabel;
    statusBar()->addWidget(m_descriptionLabel);
    statusBar()->addWidget(m_databaseLabel);
    statusBar()->addPermanentWidget(m_timeLabel);
    statusBar()->addPermanentWidget(m_loadLabel);
    statusBar()->addPermanentWidget(m_countLabel);
    auto *statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::updateStatus);
    statusTimer->start(1000);

    connect(m_tester, &Tester::runningChanged, this, &MainWindow::updateMeasurementActions);
    updateMeasurementActions();
    updateStatus();
    updateTitle();
}

void MainWindow::setupWindows()
{
    auto addWindow = [this](QWidget *widget, const QString &title, const QRect &geometry) {
        QMdiSubWindow *window = m_mdiArea->addSubWindow(widget);
        window->setWindowTitle(title);
        window->setAttribute(Qt::WA_DeleteOnClose, false); // closing only hides it
        window->setGeometry(geometry);
        return window;
    };
    m_diagnosticConsole = new DiagnosticConsole(m_tester);
    m_ecuState = new EcuStateWindow(m_tester);
    m_trace = new TraceWindow(m_tester);
    m_generator = new GeneratorWindow(m_tester);
    m_graph = new GraphWindow(m_tester);
    m_diagnosticConsoleWindow = addWindow(m_diagnosticConsole, "Diagnostic console", QRect(0, 0, 820, 520));
    m_ecuStateWindow = addWindow(m_ecuState, "ECU state", QRect(820, 0, 560, 170));
    m_graphWindow = addWindow(m_graph, "Graphics", QRect(820, 170, 560, 350));
    m_traceWindow = addWindow(m_trace, "Trace", QRect(0, 520, 820, 290));
    m_generatorWindow = addWindow(m_generator, "Interactive generator", QRect(820, 520, 560, 290));

    connect(m_trace, &TraceWindow::graphRequested, this, [this](int messageId, int signalId) {
        m_graph->addSignal(messageId, signalId);
        showWindow(m_graphWindow);
    });
}

void MainWindow::setupMenus()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(QIcon::fromTheme("document-open"), "Open &Diagnostic Description...",
                        this, &MainWindow::openDiagnosticDescription, QKeySequence::Open);
    fileMenu->addAction(QIcon::fromTheme("document-open"), "Open Data&base...",
                        this, &MainWindow::openDatabase, QKeySequence("Ctrl+Shift+O"));
    fileMenu->addAction("Open &Examples", this, &MainWindow::openExamples, QKeySequence("Ctrl+E"));
    fileMenu->addSeparator();
    fileMenu->addAction(QIcon::fromTheme("application-exit"), "&Quit", this, &QWidget::close, QKeySequence::Quit);

    QMenu *windowsMenu = menuBar()->addMenu("&Windows");
    windowsMenu->addAction("Diagnostic &Console", this, [this] { showWindow(m_diagnosticConsoleWindow); });
    windowsMenu->addAction("&ECU State", this, [this] { showWindow(m_ecuStateWindow); });
    windowsMenu->addAction("T&race", this, [this] { showWindow(m_traceWindow); });
    windowsMenu->addAction("&Interactive Generator", this, [this] { showWindow(m_generatorWindow); });
    windowsMenu->addAction("&Graphics", this, [this] { showWindow(m_graphWindow); });
    windowsMenu->addSeparator();
    windowsMenu->addAction("&Tile", m_mdiArea, &QMdiArea::tileSubWindows);
    windowsMenu->addAction("C&ascade", m_mdiArea, &QMdiArea::cascadeSubWindows);
}

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar("Measurement");
    toolBar->setObjectName("measurementToolBar");
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    m_startAction = toolBar->addAction(QIcon::fromTheme("media-playback-start"), "Start");
    m_startAction->setShortcut(QKeySequence("F9"));
    connect(m_startAction, &QAction::triggered, this, &MainWindow::startOrStopMeasurement);

    m_simulateAction = toolBar->addAction("Simulate ECU");
    m_simulateAction->setCheckable(true);
    m_simulateAction->setChecked(true);
    m_simulateAction->setToolTip("Start a simulated ECU inside DiagProbe that answers on the remote port");

    m_testerPresentAction = toolBar->addAction("TesterPresent (S3)");
    m_testerPresentAction->setCheckable(true);
    m_testerPresentAction->setChecked(m_tester->isTesterPresentEnabled());
    m_testerPresentAction->setToolTip("Keep a non-default session alive with TesterPresent (3E 80)");
    connect(m_testerPresentAction, &QAction::toggled, m_tester, &Tester::setTesterPresentEnabled);

    toolBar->addSeparator();
    toolBar->addAction("Tile", m_mdiArea, &QMdiArea::tileSubWindows);
    toolBar->addAction("Cascade", m_mdiArea, &QMdiArea::cascadeSubWindows);
}

bool MainWindow::openFiles(const QStringList &filePaths)
{
    bool ok = true;
    for (const QString &filePath : filePaths) {
        if (filePath.endsWith(".uscxml", Qt::CaseInsensitive))
            ok = loadDiagnosticDescription(filePath) && ok;
        else
            ok = loadDatabase(filePath) && ok;
    }
    return ok;
}

void MainWindow::openDiagnosticDescription()
{
    const QString filePath = QFileDialog::getOpenFileName(this, "Open Diagnostic Description", QString(),
                                                          "UDScribe files (*.uscxml);;All files (*)");
    if (!filePath.isEmpty())
        loadDiagnosticDescription(filePath);
}

void MainWindow::openDatabase()
{
    const QString filePath = QFileDialog::getOpenFileName(this, "Open Database", QString(),
                                                          "FrameForge files (*.ffxml);;All files (*)");
    if (!filePath.isEmpty())
        loadDatabase(filePath);
}

void MainWindow::openExamples()
{
    const QDir examples(EXAMPLES_DIR);
    openFiles({examples.filePath("BodyECU.uscxml"), examples.filePath("Vehicle.ffxml")});
}

bool MainWindow::loadDiagnosticDescription(const QString &filePath)
{
    m_tester->stop(); // the databases are only replaced while not measuring
    QString error;
    if (!usc::XmlStorage::load(*m_diagnosticDescription, filePath, &error)) {
        QMessageBox::warning(this, "Open Diagnostic Description", QString("Could not open %1:\n%2").arg(filePath, error));
        return false;
    }
    m_diagnosticDescriptionPath = filePath;
    const usc::Database *description = m_diagnosticDescription;
    const ff::Database *database = m_messageDatabasePath.isEmpty() ? nullptr : m_messageDatabase;
    m_tester->setDatabases(description, database);
    m_diagnosticConsole->showDescription(description, QFileInfo(filePath).fileName());
    m_trace->setDatabases(description, database);
    showWindow(m_diagnosticConsoleWindow);
    updateTitle();
    return true;
}

bool MainWindow::loadDatabase(const QString &filePath)
{
    m_tester->stop();
    QString error;
    if (!ff::XmlStorage::load(*m_messageDatabase, filePath, &error)) {
        QMessageBox::warning(this, "Open Database", QString("Could not open %1:\n%2").arg(filePath, error));
        return false;
    }
    m_messageDatabasePath = filePath;
    const usc::Database *description = m_diagnosticDescriptionPath.isEmpty() ? nullptr : m_diagnosticDescription;
    m_tester->setDatabases(description, m_messageDatabase);
    m_trace->setDatabases(description, m_messageDatabase);
    m_generator->setDatabase(m_messageDatabase);
    m_graph->setDatabase(m_messageDatabase);
    // Start with the engine speed on the graph, if there is one.
    for (const ff::Message &message : m_messageDatabase->allMessages()) {
        for (const ff::SignalMapping &mapping : message.signalMappings) {
            const ff::Signal *signal = m_messageDatabase->findSignal(mapping.signalId);
            if (signal && signal->name == "EngineSpeed" && m_graph->signalCount() == 0)
                m_graph->addSignal(message.id, signal->id);
        }
    }
    updateTitle();
    return true;
}

void MainWindow::startOrStopMeasurement()
{
    if (m_tester->isRunning()) {
        m_tester->stop();
        return;
    }
    QString error;
    if (!m_tester->start(m_simulateAction->isChecked(), &error))
        QMessageBox::warning(this, "Start Measurement", error);
}

void MainWindow::updateMeasurementActions()
{
    const bool running = m_tester->isRunning();
    m_startAction->setText(running ? "Stop" : "Start");
    m_startAction->setIcon(QIcon::fromTheme(running ? "media-playback-stop" : "media-playback-start"));
    m_startAction->setToolTip(running ? "Stop the measurement (F9)" : "Start the measurement (F9)");
    m_simulateAction->setEnabled(!running);
    m_lastFrameCount = 0;
    updateStatus();
}

void MainWindow::updateStatus()
{
    const quint64 frames = m_tester->receivedCount() + m_tester->sentCount();
    if (m_tester->isRunning()) {
        m_timeLabel->setText(QString("Time %1 s").arg(m_tester->time(), 0, 'f', 1));
        m_loadLabel->setText(QString("Load %1 msg/s").arg(frames - m_lastFrameCount));
    } else {
        m_timeLabel->setText("Stopped");
        m_loadLabel->setText("Load —");
    }
    m_countLabel->setText(QString("Rx %1  Tx %2").arg(m_tester->receivedCount()).arg(m_tester->sentCount()));
    m_lastFrameCount = frames;
}

void MainWindow::showWindow(QMdiSubWindow *window)
{
    // Closing a sub-window only hides it (it is not deleted), so it can be shown again.
    window->showNormal();
    m_mdiArea->setActiveSubWindow(window);
}

void MainWindow::updateTitle()
{
    const QString description = m_diagnosticDescriptionPath.isEmpty() ? QString("no diagnostic description")
                                                                      : QFileInfo(m_diagnosticDescriptionPath).fileName();
    const QString database = m_messageDatabasePath.isEmpty() ? QString("no database")
                                                             : QFileInfo(m_messageDatabasePath).fileName();
    setWindowTitle(QString("%1 · %2 - DiagProbe").arg(description, database));

    m_descriptionLabel->setText("ECU: " + description);
    m_databaseLabel->setText("Database: " + database);
}
