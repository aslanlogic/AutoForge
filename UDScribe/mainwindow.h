#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace usc {
class Database;
} // namespace usc

class NavigationModel;
class QSplitter;
class QStackedWidget;
class QTreeView;

using namespace usc;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void setupLayout();
    void showNavigationContextMenu(const QPoint &pos);
    void onNavigationDoubleClicked(const QModelIndex &index);
    void createNewSecurityLevel();
    void editSecurityLevel(int id);
    void deleteSecurityLevel(int id);

    Database *m_database = nullptr;
    NavigationModel *m_navigationModel = nullptr;

    QSplitter *m_splitter = nullptr;
    QTreeView *m_navigationTree = nullptr;
    QStackedWidget *m_rightPane = nullptr;
    QWidget *m_emptyPage = nullptr;
};

#endif // MAINWINDOW_H
