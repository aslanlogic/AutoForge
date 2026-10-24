#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "usccore/dataidentifier.h"

#include <QMainWindow>

namespace usc {
class Database;
} // namespace usc

class DataIdentifierPropertiesWidget;
class DataIdentifierTableModel;
class DataTypePropertiesWidget;
class DataTypeTableModel;
class NavigationModel;
class NetworkPropertiesWidget;
class QAbstractItemModel;
class QSplitter;
class QStackedWidget;
class QTableView;
class QTreeView;
class ResetTypePropertiesWidget;
class ResetTypeTableModel;
class SecurityLevelPropertiesWidget;
class SecurityLevelTableModel;
class SessionPropertiesWidget;
class SessionTableModel;
class ValueTablePropertiesWidget;
class ValueTableTableModel;

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
    // Draws the shown reset type again, or the empty page if it no longer exists.
    void refreshResetTypeProperties();
    // Draws the shown security level again, or the empty page if it no longer exists.
    void refreshSecurityLevelProperties();
    // Draws the shown value table again, or the empty page if it no longer exists.
    void refreshValueTableProperties();
    // Draws the shown data type again, or the empty page if it no longer exists.
    void refreshDataTypeProperties();
    // Draws the shown DID again, or the empty page if it no longer exists.
    void refreshDataIdentifierProperties();
    // Draws the network page again if it is shown.
    void refreshNetworkProperties();
    void showNavigationContextMenu(const QPoint &pos);
    void onNavigationDoubleClicked(const QModelIndex &index);
    void onCurrentNavigationChanged(const QModelIndex &current);
    void editNetwork();
    void createNewSession();
    void editSession(int id);
    void deleteSession(int id);
    void createNewResetType();
    void editResetType(int id);
    void deleteResetType(int id);
    void createNewDataIdentifier();
    void editDataIdentifier(int id);
    // 0x22 ReadDataByIdentifier / 0x2E WriteDataByIdentifier: make a DID readable or
    // writable, change or remove that access.
    void addAccessDataIdentifier(DidAccess access);
    void editAccess(int id, DidAccess access);
    void removeAccess(int id, DidAccess access);
    void deleteDataIdentifier(int id);
    void createNewDataType();
    void editDataType(int id);
    void deleteDataType(int id);
    void createNewValueTable();
    void editValueTable(int id);
    void deleteValueTable(int id);
    void createNewSecurityLevel();
    void editSecurityLevel(int id);
    void deleteSecurityLevel(int id);

    Database *m_database = nullptr;
    NavigationModel *m_navigationModel = nullptr;
    SessionTableModel *m_sessionTableModel = nullptr;
    ResetTypeTableModel *m_resetTypeTableModel = nullptr;
    SecurityLevelTableModel *m_securityLevelTableModel = nullptr;
    ValueTableTableModel *m_valueTableTableModel = nullptr;
    DataTypeTableModel *m_dataTypeTableModel = nullptr;
    DataIdentifierTableModel *m_dataIdentifierTableModel = nullptr;
    QString m_currentFilePath; // empty until the document is saved or opened

    QSplitter *m_splitter = nullptr;
    QTreeView *m_navigationTree = nullptr;
    QStackedWidget *m_rightPane = nullptr;
    QWidget *m_emptyPage = nullptr;
    QTableView *m_sessionTable = nullptr;
    QTableView *m_resetTypeTable = nullptr;
    QTableView *m_securityLevelTable = nullptr;
    QTableView *m_valueTableTable = nullptr;
    QTableView *m_dataTypeTable = nullptr;
    QTableView *m_dataIdentifierTable = nullptr;
    QTableView *m_readDataIdentifierTable = nullptr;  // the readable DIDs only
    QTableView *m_writeDataIdentifierTable = nullptr; // the writable DIDs only
    NetworkPropertiesWidget *m_networkProperties = nullptr;
    SessionPropertiesWidget *m_sessionProperties = nullptr;
    int m_shownSessionId = 0; // the session on m_sessionProperties; 0 = none
    ResetTypePropertiesWidget *m_resetTypeProperties = nullptr;
    int m_shownResetTypeId = 0; // the reset type on m_resetTypeProperties; 0 = none
    SecurityLevelPropertiesWidget *m_securityLevelProperties = nullptr;
    int m_shownSecurityLevelId = 0; // the level on m_securityLevelProperties; 0 = none
    ValueTablePropertiesWidget *m_valueTableProperties = nullptr;
    int m_shownValueTableId = 0; // the value table on m_valueTableProperties; 0 = none
    DataTypePropertiesWidget *m_dataTypeProperties = nullptr;
    int m_shownDataTypeId = 0; // the data type on m_dataTypeProperties; 0 = none
    DataIdentifierPropertiesWidget *m_dataIdentifierProperties = nullptr;
    int m_shownDataIdentifierId = 0; // the DID on m_dataIdentifierProperties; 0 = none
};

#endif // MAINWINDOW_H
