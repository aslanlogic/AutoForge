#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QSplitter;
class QTreeWidget;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupLayout();

    QSplitter *m_splitter = nullptr;
    QTreeWidget *m_navigationTree = nullptr;
    QWidget *m_definitionArea = nullptr;
};

#endif // MAINWINDOW_H
