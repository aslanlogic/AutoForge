#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace ff {
class Database;
} // namespace ff

namespace usc {
class Database;
} // namespace usc

class DiagnosticConsole;
class EcuStateWindow;
class GeneratorWindow;
class GraphWindow;
class QAction;
class QDir;
class QSettings;
class QLabel;
class QMdiArea;
class QMdiSubWindow;
class Tester;
class TraceWindow;

// The tester's main window: a desktop of movable windows (QMdiArea) like CANoe.
// It holds one diagnostic description (.uscxml, the ECU to talk to) and one
// message database (.ffxml, to decode the bus traffic). Both are only read.
//
// A configuration (.dpcfg, an INI file) saves the whole setup: both files (relative
// to the configuration), the window layout, the Graphics signals, the Interactive
// generator's transmit list and the toolbar switches.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Opens .dpcfg, .uscxml and .ffxml files, e.g. from the command line. Returns
    // false (after telling the user) if one could not be opened.
    bool openFiles(const QStringList &filePaths);

    Tester *tester() const { return m_tester; }

protected:
    // Asks to save the configuration if it has changed.
    void closeEvent(QCloseEvent *event) override;

private:
    void setupWindows();
    void setupMenus();
    void setupToolBar();
    void openDiagnosticDescription();
    void openDatabase();
    void openExamples();
    bool loadDiagnosticDescription(const QString &filePath);
    bool loadDatabase(const QString &filePath);
    void openConfiguration();
    bool saveConfiguration();
    bool saveConfigurationAs();
    bool loadConfiguration(const QString &filePath);
    bool saveConfigurationTo(const QString &filePath);
    void writeConfiguration(QSettings &settings, const QDir &directory) const;
    // Unsaved changes are found by comparing what would be saved with what was saved.
    QByteArray configurationSnapshot() const;
    void markConfigurationSaved();
    // Asks to save unsaved changes; false if the user cancels.
    bool maybeSaveConfiguration();
    // The sub-windows by the names a configuration uses for them.
    QList<QPair<QString, QMdiSubWindow *>> namedWindows() const;
    void startOrStopMeasurement();
    void updateMeasurementActions();
    void updateStatus();
    void showWindow(QMdiSubWindow *window);
    void updateTitle();

    // Both cores have a class called Database; the namespaces keep them apart.
    usc::Database *m_diagnosticDescription = nullptr;
    ff::Database *m_messageDatabase = nullptr;
    QString m_diagnosticDescriptionPath; // empty until one is opened
    QString m_messageDatabasePath;       // empty until one is opened
    QString m_configurationPath;         // empty until one is opened or saved
    QByteArray m_savedSnapshot;          // configurationSnapshot() when last saved or opened

    Tester *m_tester = nullptr;

    QMdiArea *m_mdiArea = nullptr;
    DiagnosticConsole *m_diagnosticConsole = nullptr;
    EcuStateWindow *m_ecuState = nullptr;
    TraceWindow *m_trace = nullptr;
    GeneratorWindow *m_generator = nullptr;
    GraphWindow *m_graph = nullptr;
    QMdiSubWindow *m_diagnosticConsoleWindow = nullptr;
    QMdiSubWindow *m_ecuStateWindow = nullptr;
    QMdiSubWindow *m_traceWindow = nullptr;
    QMdiSubWindow *m_generatorWindow = nullptr;
    QMdiSubWindow *m_graphWindow = nullptr;

    QAction *m_startAction = nullptr;
    QAction *m_testerPresentAction = nullptr;

    QLabel *m_descriptionLabel = nullptr; // in the status bar
    QLabel *m_databaseLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_loadLabel = nullptr;
    QLabel *m_countLabel = nullptr;
    quint64 m_lastFrameCount = 0; // for the bus load
};

#endif // MAINWINDOW_H
