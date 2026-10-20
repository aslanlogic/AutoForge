#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class Database;
class NavigationModel;
class QSortFilterProxyModel;
class QSplitter;
class QStackedWidget;
class QTableView;
class QTreeView;
class SignalPropertiesWidget;
class SignalTableModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void showNavigationContextMenu(const QPoint &pos);
    void createNewSignal();
    void onNavigationDoubleClicked(const QModelIndex &index);
    void editSignal(int id);
    void onCurrentNavigationChanged(const QModelIndex &current);
    void onSignalChanged(int id);
    void openFile();
    bool saveFile();
    bool saveFileAs();

private:
    void setupMenus();
    void setupLayout();
    bool saveTo(const QString &filePath);
    void setCurrentFilePath(const QString &filePath);
    // If there are unsaved changes, asks whether to save them.
    // Returns false if the user chose Cancel (or saving failed): the caller must then stop.
    bool maybeSave();
    QWidget *createSignalTable();
    void showSignalProperties(int id); // 0 = show nothing

    Database *m_database = nullptr;
    QString m_currentFilePath; // empty until the document is saved or opened
    NavigationModel *m_navigationModel = nullptr;
    SignalTableModel *m_signalTableModel = nullptr;
    QSortFilterProxyModel *m_signalSortModel = nullptr; // sorts the table without touching Database

    QSplitter *m_splitter = nullptr;
    QTreeView *m_navigationTree = nullptr;

    // Right pane: shows one page at a time.
    QStackedWidget *m_rightPane = nullptr;
    QWidget *m_emptyPage = nullptr;
    SignalPropertiesWidget *m_signalProperties = nullptr; // one signal
    QTableView *m_signalTable = nullptr;                  // all signals
    int m_shownSignalId = 0; // signal currently shown in the right pane, 0 = none
};

#endif // MAINWINDOW_H
