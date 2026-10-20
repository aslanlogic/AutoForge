#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class Database;
class MessagePropertiesWidget;
class MessageTableModel;
class NavigationFilterModel;
class NavigationModel;
class QAbstractItemModel;
class QLineEdit;
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
    // Indexes from the tree belong to m_navigationFilter, hence the "filter" prefix.
    void onNavigationDoubleClicked(const QModelIndex &filterIndex);
    void editSignal(int id);
    void createNewMessage();
    void editMessage(int id);
    void deleteSignal(int id);  // asks first
    void deleteMessage(int id); // asks first
    void onCurrentNavigationChanged(const QModelIndex &filterCurrent);
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
    QTableView *createSortableTable(QAbstractItemModel *model, int sortRole);
    void showEmptyPage();
    void showSignalProperties(int id);  // shows the empty page if there is no such signal
    void showMessageProperties(int id); // shows the empty page if there is no such message
    void showTable(QTableView *table);

    Database *m_database = nullptr;
    QString m_currentFilePath; // empty until the document is saved or opened
    NavigationModel *m_navigationModel = nullptr;
    NavigationFilterModel *m_navigationFilter = nullptr; // applies the search text
    SignalTableModel *m_signalTableModel = nullptr;
    MessageTableModel *m_messageTableModel = nullptr;

    QSplitter *m_splitter = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QTreeView *m_navigationTree = nullptr;

    // Right pane: shows one page at a time.
    QStackedWidget *m_rightPane = nullptr;
    QWidget *m_emptyPage = nullptr;
    SignalPropertiesWidget *m_signalProperties = nullptr;   // one signal
    QTableView *m_signalTable = nullptr;                    // all signals
    MessagePropertiesWidget *m_messageProperties = nullptr; // one message
    QTableView *m_messageTable = nullptr;                   // all messages
    // What the right pane currently shows, so it can be refreshed after edits; 0 = none.
    int m_shownSignalId = 0;
    int m_shownMessageId = 0;
};

#endif // MAINWINDOW_H
