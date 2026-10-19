#include "mainwindow.h"
#include "database.h"
#include "navigationmodel.h"
#include "signaldialog.h"

#include <QIcon>
#include <QMenu>
#include <QSplitter>
#include <QStyle>
#include <QTreeView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("FrameForge");
    resize(1024, 768);

    m_database = new Database(this);
    m_navigationModel = new NavigationModel(m_database, this);
    setupLayout();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupLayout()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Left pane: QTreeView scrolls on its own when content overflows.
    m_navigationTree = new QTreeView(m_splitter);
    m_navigationTree->setHeaderHidden(true);
    m_navigationTree->setModel(m_navigationModel);

    // Show newly added items by expanding their heading.
    connect(m_navigationModel, &NavigationModel::rowsInserted,
            m_navigationTree, &QTreeView::expand);

    m_navigationTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_navigationTree, &QTreeView::customContextMenuRequested,
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
    const QModelIndex index = m_navigationTree->indexAt(pos);
    if (index != m_navigationModel->categoryIndex(NavigationModel::Signals))
        return;

    QMenu menu(this);
    // Use the desktop theme's icon, falling back to Qt's built-in one where no theme exists (e.g. Windows).
    const QIcon newIcon = QIcon::fromTheme("document-new", style()->standardIcon(QStyle::SP_FileIcon));
    menu.addAction(newIcon, "New", this, &MainWindow::createNewSignal);
    menu.exec(m_navigationTree->viewport()->mapToGlobal(pos));
}

void MainWindow::createNewSignal()
{
    const QStringList existingNames = m_database->signalNames();

    // Pick the first free default name: New_Signal_1, New_Signal_2, ...
    Signal newSignal;
    int number = 1;
    do {
        newSignal.name = QString("New_Signal_%1").arg(number++);
    } while (existingNames.contains(newSignal.name));

    SignalDialog dialog(newSignal, existingNames, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // The tree updates itself: Database -> NavigationModel -> QTreeView.
    m_database->addSignal(dialog.signal());
}
