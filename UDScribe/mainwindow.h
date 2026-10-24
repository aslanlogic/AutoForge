#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace usc {
class Database;
} // namespace usc

class NavigationModel;
class NetworkPropertiesWidget;
class QAbstractItemModel;
class QSplitter;
class QStackedWidget;
class QTableView;
class QTreeView;
class SecurityLevelPropertiesWidget;
class SecurityLevelTableModel;
class SessionPropertiesWidget;
class SessionTableModel;

using namespace usc;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupMenus();
    void setupLayout();
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    bool saveTo(const QString &filePath);
    void setCurrentFilePath(const QString &filePath);
    // If there are unsaved changes, asks whether to save them.
    // Returns false if the user chose Cancel (or saving failed): the caller must then stop.
    bool maybeSave();
    QTableView *createSortableTable(QAbstractItemModel *model, int sortRole);
    // Draws the shown session again, or the empty page if it no longer exists.
    void refreshSessionProperties();
    // Draws the shown security level again, or the empty page if it no longer exists.
    void refreshSecurityLevelProperties();
    // Draws the network page again if it is shown.
    void refreshNetworkProperties();
    void showNavigationContextMenu(const QPoint &pos);
    void onNavigationDoubleClicked(const QModelIndex &index);
    void onCurrentNavigationChanged(const QModelIndex &current);
    void editNetwork();
    void createNewSession();
    void editSession(int id);
    void deleteSession(int id);
    void createNewSecurityLevel();
    void editSecurityLevel(int id);
    void deleteSecurityLevel(int id);

    Database *m_database = nullptr;
    NavigationModel *m_navigationModel = nullptr;
    SessionTableModel *m_sessionTableModel = nullptr;
    SecurityLevelTableModel *m_securityLevelTableModel = nullptr;
    QString m_currentFilePath; // empty until the document is saved or opened

    QSplitter *m_splitter = nullptr;
    QTreeView *m_navigationTree = nullptr;
    QStackedWidget *m_rightPane = nullptr;
    QWidget *m_emptyPage = nullptr;
    QTableView *m_sessionTable = nullptr;
    QTableView *m_securityLevelTable = nullptr;
    NetworkPropertiesWidget *m_networkProperties = nullptr;
    SessionPropertiesWidget *m_sessionProperties = nullptr;
    int m_shownSessionId = 0; // the session on m_sessionProperties; 0 = none
    SecurityLevelPropertiesWidget *m_securityLevelProperties = nullptr;
    int m_shownSecurityLevelId = 0; // the level on m_securityLevelProperties; 0 = none
};

#endif // MAINWINDOW_H
