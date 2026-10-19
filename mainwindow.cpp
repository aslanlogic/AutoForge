#include "mainwindow.h"

#include <QSplitter>
#include <QTreeWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("FrameForge");
    resize(1024, 768);

    setupLayout();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupLayout()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Left pane: QTreeWidget scrolls on its own when content overflows.
    m_navigationTree = new QTreeWidget(m_splitter);
    m_navigationTree->setHeaderHidden(true);
    const QStringList headings = {"Network", "Network Nodes", "Messages", "Signals"};
    for (const QString &heading : headings)
        new QTreeWidgetItem(m_navigationTree, {heading});

    // Right pane: will show the definition of the selected item.
    m_definitionArea = new QWidget(m_splitter);

    m_splitter->addWidget(m_navigationTree);
    m_splitter->addWidget(m_definitionArea);
    m_splitter->setChildrenCollapsible(false);

    // Start with the left pane at 20% of the window width.
    const int leftWidth = width() * 20 / 100;
    m_splitter->setSizes({leftWidth, width() - leftWidth});

    setCentralWidget(m_splitter);
}
