#include "mainwindow.h"
#include "database.h"
#include "navigationmodel.h"
#include "signaldialog.h"
#include "signalpropertieswidget.h"
#include "signaltablemodel.h"
#include "xmlstorage.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTableView>
#include <QTreeView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1024, 768);

    m_database = new Database(this);
    m_navigationModel = new NavigationModel(m_database, this);
    setupMenus();
    setupLayout();
    setCurrentFilePath(QString());

    // Any edit marks the document as unsaved: Qt then shows '*' in place of "[*]" in the title.
    // Opening a file (Database::wasReset) is not an edit, so it is not connected here.
    connect(m_database, &Database::signalAdded, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::signalChanged, this, [this] { setWindowModified(true); });
}

void MainWindow::setupMenus()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(QIcon::fromTheme("document-open"), "&Open...",
                        this, &MainWindow::openFile, QKeySequence::Open);
    fileMenu->addAction(QIcon::fromTheme("document-save"), "&Save",
                        this, &MainWindow::saveFile, QKeySequence::Save);
    fileMenu->addAction(QIcon::fromTheme("document-save-as"), "Save &As...",
                        this, &MainWindow::saveFileAs, QKeySequence("Ctrl+Shift+S"));
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
    connect(m_navigationTree, &QTreeView::doubleClicked,
            this, &MainWindow::onNavigationDoubleClicked);

    // Right pane: shows what is clicked in the tree.
    m_rightPane = new QStackedWidget(m_splitter);
    m_emptyPage = new QWidget;
    m_signalProperties = new SignalPropertiesWidget;
    m_rightPane->addWidget(m_emptyPage);
    m_rightPane->addWidget(m_signalProperties);
    m_rightPane->addWidget(createSignalTable());
    connect(m_navigationTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onCurrentNavigationChanged);
    connect(m_database, &Database::signalChanged, this, &MainWindow::onSignalChanged);
    // After a file is opened nothing is selected any more: clear the right pane and show the signals.
    connect(m_database, &Database::wasReset, this, [this] {
        showSignalProperties(0);
        m_navigationTree->expand(m_navigationModel->categoryIndex(NavigationModel::Signals));
    });

    m_splitter->addWidget(m_navigationTree);
    m_splitter->addWidget(m_rightPane);
    m_splitter->setChildrenCollapsible(false);

    // Start with the left pane at 20% of the window width.
    const int leftWidth = width() * 20 / 100;
    m_splitter->setSizes({leftWidth, width() - leftWidth});

    setCentralWidget(m_splitter);
}

QWidget *MainWindow::createSignalTable()
{
    // Database -> SignalTableModel -> sort proxy -> QTableView.
    // Clicking a column header sorts the view only; Database keeps its own order.
    m_signalTableModel = new SignalTableModel(m_database, this);
    m_signalSortModel = new QSortFilterProxyModel(this);
    m_signalSortModel->setSourceModel(m_signalTableModel);
    m_signalSortModel->setSortRole(SignalTableModel::SortRole);
    m_signalSortModel->setSortCaseSensitivity(Qt::CaseInsensitive);

    m_signalTable = new QTableView;
    m_signalTable->setModel(m_signalSortModel);
    m_signalTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_signalTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_signalTable->verticalHeader()->setVisible(false);
    m_signalTable->horizontalHeader()->setStretchLastSection(true);

    // Start in creation order (no sort column); a header click sorts by that column.
    m_signalTable->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    m_signalTable->setSortingEnabled(true);

    return m_signalTable;
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

void MainWindow::onNavigationDoubleClicked(const QModelIndex &index)
{
    if (index.parent() == m_navigationModel->categoryIndex(NavigationModel::Signals))
        editSignal(index.data(NavigationModel::ObjectIdRole).toInt());
}

void MainWindow::editSignal(int id)
{
    const Signal *stored = m_database->findSignal(id);
    if (!stored)
        return;
    const Signal signal = *stored; // copy: the pointer is only valid until the next change

    // Keeping its own name is fine; taking another signal's name is not.
    QStringList otherNames = m_database->signalNames();
    otherNames.removeOne(signal.name);

    SignalDialog dialog(signal, otherNames, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->updateSignal(dialog.signal());
}

void MainWindow::onCurrentNavigationChanged(const QModelIndex &current)
{
    const QModelIndex signalsHeading = m_navigationModel->categoryIndex(NavigationModel::Signals);

    if (current == signalsHeading) {
        m_shownSignalId = 0;
        m_rightPane->setCurrentWidget(m_signalTable);
    } else if (current.parent() == signalsHeading) {
        showSignalProperties(current.data(NavigationModel::ObjectIdRole).toInt());
    } else {
        showSignalProperties(0);
    }
}

void MainWindow::onSignalChanged(int id)
{
    // Keep the right pane up to date when the shown signal is edited.
    if (id == m_shownSignalId)
        showSignalProperties(id);
}

void MainWindow::showSignalProperties(int id)
{
    const Signal *signal = m_database->findSignal(id);
    m_shownSignalId = signal ? id : 0;

    if (signal) {
        m_signalProperties->showSignal(*signal);
        m_rightPane->setCurrentWidget(m_signalProperties);
    } else {
        m_rightPane->setCurrentWidget(m_emptyPage);
    }
}

namespace {
const QString FileFilter = "FrameForge XML (*.xml);;All files (*)";
}

void MainWindow::openFile()
{
    if (!maybeSave())
        return;

    const QString filePath = QFileDialog::getOpenFileName(this, "Open", QString(), FileFilter);
    if (filePath.isEmpty())
        return;

    QString error;
    if (!XmlStorage::load(*m_database, filePath, &error)) {
        QMessageBox::warning(this, "Open", QString("Could not open %1:\n%2").arg(filePath, error));
        return;
    }
    setCurrentFilePath(filePath);
    setWindowModified(false);
}

bool MainWindow::saveFile()
{
    if (m_currentFilePath.isEmpty())
        return saveFileAs();
    return saveTo(m_currentFilePath);
}

bool MainWindow::saveFileAs()
{
    QString filePath = QFileDialog::getSaveFileName(this, "Save As", m_currentFilePath, FileFilter);
    if (filePath.isEmpty())
        return false;
    if (QFileInfo(filePath).suffix().isEmpty())
        filePath += ".xml";

    return saveTo(filePath);
}

bool MainWindow::saveTo(const QString &filePath)
{
    QString error;
    if (!XmlStorage::save(*m_database, filePath, &error)) {
        QMessageBox::warning(this, "Save", QString("Could not save %1:\n%2").arg(filePath, error));
        return false;
    }
    setCurrentFilePath(filePath);
    setWindowModified(false);
    return true;
}

void MainWindow::setCurrentFilePath(const QString &filePath)
{
    m_currentFilePath = filePath;
    const QString fileName = filePath.isEmpty() ? "Untitled" : QFileInfo(filePath).fileName();
    setWindowTitle(QString("%1[*] - FrameForge").arg(fileName));
}

bool MainWindow::maybeSave()
{
    if (!isWindowModified())
        return true;

    const auto answer = QMessageBox::warning(
        this, "FrameForge", "The document has unsaved changes.\nDo you want to save them?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);

    if (answer == QMessageBox::Save)
        return saveFile(); // false if the user cancels Save As or saving fails
    return answer == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave())
        event->accept();
    else
        event->ignore();
}
