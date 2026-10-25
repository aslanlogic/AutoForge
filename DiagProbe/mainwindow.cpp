#include "mainwindow.h"
#include "desktoptabs.h"
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
#include <QMenu>
#include <QMenuBar>
#include <QCloseEvent>
#include <QSettings>
#include <QTemporaryDir>
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
    m_tester->setKeyAlgorithm(&m_keyAlgorithm);

    m_desktops = new DesktopTabs;
    m_desktops->addDesktop("Desktop 1");
    setCentralWidget(m_desktops);

    setupWindows();
    setupMenus();
    setupToolBar();
    // Shortcuts (F9, Ctrl+S, ...) work in desktops moved to a separate window too.
    QList<QAction *> shortcuts;
    for (QAction *action : findChildren<QAction *>()) {
        if (!action->shortcut().isEmpty())
            shortcuts << action;
    }
    m_desktops->setSharedActions(shortcuts);

    m_descriptionLabel = new QLabel;
    m_databaseLabel = new QLabel;
    m_keyAlgorithmLabel = new QLabel;
    m_timeLabel = new QLabel;
    m_loadLabel = new QLabel;
    m_countLabel = new QLabel;
    statusBar()->addWidget(m_descriptionLabel);
    statusBar()->addWidget(m_databaseLabel);
    statusBar()->addWidget(m_keyAlgorithmLabel);
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
    // What there is once the window is up counts as saved: closing right away asks nothing.
    QTimer::singleShot(0, this, &MainWindow::markConfigurationSaved);
}

void MainWindow::setupWindows()
{
    auto addWindow = [this](QWidget *widget, const QString &title, const QRect &geometry) {
        QMdiSubWindow *window = m_desktops->desktopArea(0)->addSubWindow(widget);
        window->setWindowTitle(title);
        window->setAttribute(Qt::WA_DeleteOnClose, false); // closing only hides it
        window->setGeometry(geometry);
        // The window's own menu (its title bar icon) can move it to another desktop.
        QMenu *moveMenu = window->systemMenu()->addMenu("Move to &Desktop");
        connect(moveMenu, &QMenu::aboutToShow, this, [this, moveMenu, window] {
            m_desktops->fillMoveMenu(moveMenu, window);
        });
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
    fileMenu->addAction("Select &Key Algorithm...", this, &MainWindow::selectKeyAlgorithm, QKeySequence("Ctrl+K"));
    fileMenu->addSeparator();
    fileMenu->addAction(QIcon::fromTheme("document-open"), "Open &Configuration...",
                        this, &MainWindow::openConfiguration, QKeySequence("Ctrl+Shift+L"));
    fileMenu->addAction(QIcon::fromTheme("document-save"), "&Save Configuration",
                        this, &MainWindow::saveConfiguration, QKeySequence::Save);
    fileMenu->addAction(QIcon::fromTheme("document-save-as"), "Save Configuration &As...",
                        this, &MainWindow::saveConfigurationAs, QKeySequence::SaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(QIcon::fromTheme("application-exit"), "&Quit", this, &QWidget::close, QKeySequence::Quit);

    QMenu *windowsMenu = menuBar()->addMenu("&Windows");
    windowsMenu->addAction("Diagnostic &Console", this, [this] { showWindow(m_diagnosticConsoleWindow); });
    windowsMenu->addAction("&ECU State", this, [this] { showWindow(m_ecuStateWindow); });
    windowsMenu->addAction("T&race", this, [this] { showWindow(m_traceWindow); });
    windowsMenu->addAction("&Interactive Generator", this, [this] { showWindow(m_generatorWindow); });
    windowsMenu->addAction("&Graphics", this, [this] { showWindow(m_graphWindow); });
    windowsMenu->addSeparator();
    QMenu *addMenu = windowsMenu->addMenu("A&dd Window to This Desktop");
    connect(addMenu, &QMenu::aboutToShow, this, [this, addMenu] {
        m_desktops->fillAddMenu(addMenu, m_desktops->currentIndex());
    });
    QMenu *moveMenu = windowsMenu->addMenu("&Move Active Window to Desktop");
    connect(moveMenu, &QMenu::aboutToShow, this, [this, moveMenu] {
        m_desktops->fillMoveMenu(moveMenu, m_desktops->currentArea()->activeSubWindow());
    });
    windowsMenu->addSeparator();
    windowsMenu->addAction("&Tile", this, [this] { m_desktops->currentArea()->tileSubWindows(); });
    windowsMenu->addAction("C&ascade", this, [this] { m_desktops->currentArea()->cascadeSubWindows(); });
}

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar("Measurement");
    toolBar->setObjectName("measurementToolBar");
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    m_startAction = toolBar->addAction(QIcon::fromTheme("media-playback-start"), "Start");
    m_startAction->setShortcut(QKeySequence("F9"));
    connect(m_startAction, &QAction::triggered, this, &MainWindow::startOrStopMeasurement);


    m_testerPresentAction = toolBar->addAction("TesterPresent (S3)");
    m_testerPresentAction->setCheckable(true);
    m_testerPresentAction->setChecked(m_tester->isTesterPresentEnabled());
    m_testerPresentAction->setToolTip("Keep a non-default session alive with TesterPresent (3E 80)");
    connect(m_testerPresentAction, &QAction::toggled, m_tester, &Tester::setTesterPresentEnabled);

    toolBar->addSeparator();
    toolBar->addAction("Tile", this, [this] { m_desktops->currentArea()->tileSubWindows(); });
    toolBar->addAction("Cascade", this, [this] { m_desktops->currentArea()->cascadeSubWindows(); });
}

bool MainWindow::openFiles(const QStringList &filePaths)
{
    bool ok = true;
    for (const QString &filePath : filePaths) {
        if (filePath.endsWith(".dpcfg", Qt::CaseInsensitive))
            ok = loadConfiguration(filePath) && ok;
        else if (filePath.endsWith(".uscxml", Qt::CaseInsensitive))
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

void MainWindow::selectKeyAlgorithm()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, "Select Key Algorithm", m_keyAlgorithm.filePath(),
        "Key algorithm plugins (*.so *.dll *.dylib);;All files (*)");
    if (!filePath.isEmpty())
        loadKeyAlgorithm(filePath);
}

bool MainWindow::loadKeyAlgorithm(const QString &filePath)
{
    QString error;
    if (!m_keyAlgorithm.load(filePath, &error)) {
        QMessageBox::warning(this, "Select Key Algorithm", QString("Could not load %1:\n%2").arg(filePath, error));
        updateTitle();
        return false;
    }
    updateTitle();
    return true;
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

QList<QPair<QString, QMdiSubWindow *>> MainWindow::namedWindows() const
{
    return {{"DiagnosticConsole", m_diagnosticConsoleWindow}, {"EcuState", m_ecuStateWindow},
            {"Graphics", m_graphWindow}, {"Trace", m_traceWindow}, {"InteractiveGenerator", m_generatorWindow}};
}

void MainWindow::openConfiguration()
{
    if (!maybeSaveConfiguration())
        return;
    const QString filePath = QFileDialog::getOpenFileName(this, "Open Configuration", QString(),
                                                          "DiagProbe configurations (*.dpcfg);;All files (*)");
    if (!filePath.isEmpty())
        loadConfiguration(filePath);
}

bool MainWindow::saveConfiguration()
{
    if (m_configurationPath.isEmpty())
        return saveConfigurationAs();
    return saveConfigurationTo(m_configurationPath);
}

bool MainWindow::saveConfigurationAs()
{
    QString filePath = QFileDialog::getSaveFileName(this, "Save Configuration", m_configurationPath,
                                                    "DiagProbe configurations (*.dpcfg)");
    if (filePath.isEmpty())
        return false;
    if (!filePath.endsWith(".dpcfg", Qt::CaseInsensitive))
        filePath += ".dpcfg";
    return saveConfigurationTo(filePath);
}

bool MainWindow::saveConfigurationTo(const QString &filePath)
{
    QFile::remove(filePath); // QSettings would otherwise keep keys that are no longer used
    QSettings settings(filePath, QSettings::IniFormat);
    writeConfiguration(settings, QFileInfo(filePath).absoluteDir());
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        QMessageBox::warning(this, "Save Configuration", QString("Could not save %1.").arg(filePath));
        return false;
    }
    m_configurationPath = filePath;
    markConfigurationSaved();
    updateTitle();
    statusBar()->showMessage("Saved " + QFileInfo(filePath).fileName(), 3000);
    return true;
}

bool MainWindow::loadConfiguration(const QString &filePath)
{
    QSettings settings(filePath, QSettings::IniFormat);
    if (!QFileInfo::exists(filePath) || settings.status() != QSettings::NoError
        || settings.value("DiagProbe/version").toInt() < 1) {
        QMessageBox::warning(this, "Open Configuration",
                             QString("Could not open %1:\nThis is not a DiagProbe configuration.").arg(filePath));
        return false;
    }
    m_tester->stop();
    const QDir directory = QFileInfo(filePath).absoluteDir();
    settings.beginGroup("DiagProbe");
    const QString description = settings.value("diagnosticDescription").toString();
    const QString database = settings.value("database").toString();
    const QString keyAlgorithm = settings.value("keyAlgorithm").toString();
    if (!description.isEmpty())
        loadDiagnosticDescription(QDir::cleanPath(directory.absoluteFilePath(description)));
    if (!database.isEmpty())
        loadDatabase(QDir::cleanPath(directory.absoluteFilePath(database)));
    if (!keyAlgorithm.isEmpty())
        loadKeyAlgorithm(QDir::cleanPath(directory.absoluteFilePath(keyAlgorithm)));
    m_testerPresentAction->setChecked(settings.value("testerPresent", true).toBool());
    restoreGeometry(settings.value("mainWindow").toByteArray());
    settings.endGroup();

    // The desktops first, then each window onto its desktop. An older configuration
    // has no desktops: everything is on one.
    m_desktops->setDesktops(settings.value("Desktops/names").toStringList());
    for (const auto &named : namedWindows()) {
        settings.beginGroup("Window." + named.first);
        QMdiSubWindow *window = named.second;
        m_desktops->moveWindow(window, qBound(0, settings.value("desktop", 0).toInt(), m_desktops->desktopCount() - 1));
        if (settings.contains("geometry")) {
            window->widget()->show();
            window->showNormal();
            window->setGeometry(settings.value("geometry").toRect());
            if (settings.value("maximized").toBool())
                window->showMaximized();
            else if (settings.value("minimized").toBool())
                window->showMinimized();
            if (!settings.value("visible", true).toBool())
                window->close(); // only hides it
        }
        settings.endGroup();
    }
    m_desktops->restoreDetached(settings); // after the windows are on their desktops
    m_desktops->setCurrentIndex(qBound(0, settings.value("Desktops/current", 0).toInt(), m_desktops->count() - 1));
    settings.beginGroup("Graphics");
    m_graph->loadSettings(settings);
    settings.endGroup();
    settings.beginGroup("InteractiveGenerator");
    m_generator->loadSettings(settings);
    settings.endGroup();

    m_configurationPath = filePath;
    markConfigurationSaved();
    updateTitle();
    return true;
}

void MainWindow::writeConfiguration(QSettings &settings, const QDir &directory) const
{
    // Paths are saved relative to the configuration, so the setup can move with its files.
    settings.beginGroup("DiagProbe");
    settings.setValue("version", 1);
    settings.setValue("diagnosticDescription", m_diagnosticDescriptionPath.isEmpty()
                          ? QString() : directory.relativeFilePath(m_diagnosticDescriptionPath));
    settings.setValue("database", m_messageDatabasePath.isEmpty()
                          ? QString() : directory.relativeFilePath(m_messageDatabasePath));
    settings.setValue("keyAlgorithm", m_keyAlgorithm.filePath().isEmpty()
                          ? QString() : directory.relativeFilePath(m_keyAlgorithm.filePath()));
    settings.setValue("testerPresent", m_testerPresentAction->isChecked());
    settings.setValue("mainWindow", saveGeometry());
    settings.endGroup();

    settings.setValue("Desktops/names", m_desktops->desktopNames());
    settings.setValue("Desktops/current", m_desktops->currentIndex());
    m_desktops->saveDetached(settings);
    for (const auto &named : namedWindows()) {
        settings.beginGroup("Window." + named.first);
        settings.setValue("desktop", m_desktops->desktopOf(named.second));
        settings.setValue("geometry", named.second->geometry());
        // isHidden, not isVisible: a window on a desktop that is not shown is still open.
        settings.setValue("visible", !named.second->isHidden());
        settings.setValue("minimized", named.second->isMinimized());
        settings.setValue("maximized", named.second->isMaximized());
        settings.endGroup();
    }
    settings.beginGroup("Graphics");
    m_graph->saveSettings(settings);
    settings.endGroup();
    settings.beginGroup("InteractiveGenerator");
    m_generator->saveSettings(settings);
    settings.endGroup();
}

QByteArray MainWindow::configurationSnapshot() const
{
    // What saving would write now, to compare with what was saved. The main window's
    // own geometry is left out: it changes by itself when the window first appears.
    QTemporaryDir temporary;
    const QString filePath = temporary.filePath("snapshot.dpcfg");
    {
        QSettings settings(filePath, QSettings::IniFormat);
        const QDir directory = m_configurationPath.isEmpty() ? QDir::root()
                                                             : QFileInfo(m_configurationPath).absoluteDir();
        writeConfiguration(settings, directory);
        settings.remove("DiagProbe/mainWindow");
    }
    QFile file(filePath);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void MainWindow::markConfigurationSaved()
{
    m_savedSnapshot = configurationSnapshot();
}

bool MainWindow::maybeSaveConfiguration()
{
    if (configurationSnapshot() == m_savedSnapshot)
        return true;
    const QString name = m_configurationPath.isEmpty() ? QString("The configuration")
                                                       : QFileInfo(m_configurationPath).fileName();
    const auto answer = QMessageBox::question(this, "DiagProbe",
                                              name + " has unsaved changes.\nDo you want to save them?",
                                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                              QMessageBox::Save);
    if (answer == QMessageBox::Save)
        return saveConfiguration();
    return answer == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSaveConfiguration()) {
        m_tester->stop();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::startOrStopMeasurement()
{
    if (m_tester->isRunning()) {
        m_tester->stop();
        return;
    }
    QString error;
    if (!m_tester->start(&error))
        QMessageBox::warning(this, "Start Measurement", error);
}

void MainWindow::updateMeasurementActions()
{
    const bool running = m_tester->isRunning();
    m_startAction->setText(running ? "Stop" : "Start");
    m_startAction->setIcon(QIcon::fromTheme(running ? "media-playback-stop" : "media-playback-start"));
    m_startAction->setToolTip(running ? "Stop the measurement (F9)" : "Start the measurement (F9)");
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
    m_desktops->showWindow(window);
}

void MainWindow::updateTitle()
{
    const QString description = m_diagnosticDescriptionPath.isEmpty() ? QString("no diagnostic description")
                                                                      : QFileInfo(m_diagnosticDescriptionPath).fileName();
    const QString database = m_messageDatabasePath.isEmpty() ? QString("no database")
                                                             : QFileInfo(m_messageDatabasePath).fileName();
    const QString configuration = m_configurationPath.isEmpty() ? QString()
                                                                : QFileInfo(m_configurationPath).fileName() + " — ";
    setWindowTitle(QString("%1%2 · %3 - DiagProbe").arg(configuration, description, database));

    m_descriptionLabel->setText("ECU: " + description);
    m_databaseLabel->setText("Database: " + database);
    m_keyAlgorithmLabel->setText("Key algorithm: " + (m_keyAlgorithm.isLoaded()
                                                          ? QFileInfo(m_keyAlgorithm.filePath()).fileName()
                                                          : QString("none")));
}
