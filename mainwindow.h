#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class Database;
class MessagePropertiesWidget;
class MessageTableModel;
class NavigationFilterModel;
class NavigationModel;
class NetworkPropertiesWidget;
class NodePropertiesWidget;
class NodeTableModel;
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
    void createNewNode();
    void editNode(int id);
    void deleteNode(int id);    // asks first
    void onCurrentNavigationChanged(const QModelIndex &filterCurrent);
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

    // The right pane shows one of these at a time.
    enum class View { Empty, Network, Signal, Message, Node, Table };
    void showView(View view, int id = 0); // id: the signal, message or node to show
    void showTable(QTableView *table);
    // Draws the shown view again from the Database. Called after every change;
    // falls back to the empty page if the shown object no longer exists.
    void refreshRightPane();

    Database *m_database = nullptr;
    QString m_currentFilePath; // empty until the document is saved or opened
    NavigationModel *m_navigationModel = nullptr;
    NavigationFilterModel *m_navigationFilter = nullptr; // applies the search text
    SignalTableModel *m_signalTableModel = nullptr;
    MessageTableModel *m_messageTableModel = nullptr;
    NodeTableModel *m_nodeTableModel = nullptr;

    QSplitter *m_splitter = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QTreeView *m_navigationTree = nullptr;

    // Right pane: shows one page at a time.
    QStackedWidget *m_rightPane = nullptr;
    QWidget *m_emptyPage = nullptr;
    NetworkPropertiesWidget *m_networkProperties = nullptr; // overview of everything
    SignalPropertiesWidget *m_signalProperties = nullptr;   // one signal
    QTableView *m_signalTable = nullptr;                    // all signals
    MessagePropertiesWidget *m_messageProperties = nullptr; // one message
    QTableView *m_messageTable = nullptr;                   // all messages
    NodePropertiesWidget *m_nodeProperties = nullptr;       // one node
    QTableView *m_nodeTable = nullptr;                      // all nodes
    View m_shownView = View::Empty;
    int m_shownId = 0;
};

#endif // MAINWINDOW_H
