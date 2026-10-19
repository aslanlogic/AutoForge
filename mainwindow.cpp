#include "mainwindow.h"
#include "signaldialog.h"

#include <QIcon>
#include <QMenu>
#include <QSplitter>
#include <QStyle>
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
    m_signalsItem = m_navigationTree->findItems("Signals", Qt::MatchExactly).value(0);

    m_navigationTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_navigationTree, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::showNavigationContextMenu);

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

void MainWindow::showNavigationContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_navigationTree->itemAt(pos);
    if (!item || item != m_signalsItem)
        return;

    QMenu menu(this);
    // Use the desktop theme's icon, falling back to Qt's built-in one where no theme exists (e.g. Windows).
    const QIcon newIcon = QIcon::fromTheme("document-new", style()->standardIcon(QStyle::SP_FileIcon));
    menu.addAction(newIcon, "New", this, &MainWindow::createNewSignal);
    menu.exec(m_navigationTree->viewport()->mapToGlobal(pos));
}

void MainWindow::createNewSignal()
{
    SignalDialog dialog("New_Signal", this);
    dialog.exec();
}
