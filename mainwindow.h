#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class Database;
class NavigationModel;
class QSplitter;
class QTreeView;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void showNavigationContextMenu(const QPoint &pos);
    void createNewSignal();

private:
    void setupLayout();

    Database *m_database = nullptr;
    NavigationModel *m_navigationModel = nullptr;

    QSplitter *m_splitter = nullptr;
    QTreeView *m_navigationTree = nullptr;
    QWidget *m_definitionArea = nullptr;
};

#endif // MAINWINDOW_H
