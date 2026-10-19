#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;
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

    QSplitter *m_splitter = nullptr;
    QTreeWidget *m_navigationTree = nullptr;
    QTreeWidgetItem *m_signalsItem = nullptr;
    QWidget *m_definitionArea = nullptr;
};

#endif // MAINWINDOW_H
