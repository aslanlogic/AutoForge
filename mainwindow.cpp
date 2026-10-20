#include "mainwindow.h"
#include "database.h"
#include "messagedialog.h"
#include "messagepropertieswidget.h"
#include "messagetablemodel.h"
#include "navigationfiltermodel.h"
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
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTableView>
#include <QTreeView>
#include <QVBoxLayout>

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
    connect(m_database, &Database::messageAdded, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::messageChanged, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::signalRemoved, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::messageRemoved, this, [this] { setWindowModified(true); });
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

    // Left pane: search box above the tree.
    auto *leftPane = new QWidget(m_splitter);
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("Search...");
    m_searchEdit->setClearButtonEnabled(true);

    // QTreeView scrolls on its own when content overflows.
    m_navigationFilter = new NavigationFilterModel(this);
    m_navigationFilter->setSourceModel(m_navigationModel);
    m_navigationFilter->sort(0); // stays sorted as signals are added or renamed
    m_navigationTree = new QTreeView;
    m_navigationTree->setHeaderHidden(true);
    m_navigationTree->setModel(m_navigationFilter);

    auto *leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(m_searchEdit);
    leftLayout->addWidget(m_navigationTree);

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_navigationFilter->setFilterFixedString(text);
        // Open every heading so the matches are visible.
        if (!text.isEmpty())
            m_navigationTree->expandAll();
    });

    // Show newly added items by expanding their heading.
    connect(m_navigationFilter, &NavigationFilterModel::rowsInserted,
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
    m_messageProperties = new MessagePropertiesWidget;
    m_signalTableModel = new SignalTableModel(m_database, this);
    m_messageTableModel = new MessageTableModel(m_database, this);
    m_signalTable = createSortableTable(m_signalTableModel, SignalTableModel::SortRole);
    m_messageTable = createSortableTable(m_messageTableModel, MessageTableModel::SortRole);
    m_rightPane->addWidget(m_emptyPage);
    m_rightPane->addWidget(m_signalProperties);
    m_rightPane->addWidget(m_signalTable);
    m_rightPane->addWidget(m_messageProperties);
    m_rightPane->addWidget(m_messageTable);
    connect(m_navigationTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onCurrentNavigationChanged);

    // Keep the right pane up to date when what it shows is edited or deleted.
    connect(m_database, &Database::signalChanged, this, &MainWindow::onSignalChanged);
    connect(m_database, &Database::signalRemoved, this, [this](int id) {
        if (id == m_shownSignalId)
            showEmptyPage();
    });
    connect(m_database, &Database::messageChanged, this, [this](int id) {
        if (id == m_shownMessageId)
            showMessageProperties(id);
    });
    connect(m_database, &Database::messageRemoved, this, [this](int id) {
        if (id == m_shownMessageId)
            showEmptyPage();
    });
    // After a file is opened nothing is selected any more: clear the right pane and show the signals.
    connect(m_database, &Database::wasReset, this, [this] {
        showEmptyPage();
        m_navigationTree->expand(
            m_navigationFilter->mapFromSource(m_navigationModel->categoryIndex(NavigationModel::Signals)));
    });

    m_splitter->addWidget(leftPane);
    m_splitter->addWidget(m_rightPane);
    m_splitter->setChildrenCollapsible(false);

    // Start with the left pane at 20% of the window width.
    const int leftWidth = width() * 20 / 100;
    m_splitter->setSizes({leftWidth, width() - leftWidth});

    setCentralWidget(m_splitter);
}

QTableView *MainWindow::createSortableTable(QAbstractItemModel *model, int sortRole)
{
    // model -> sort proxy -> QTableView.
    // Clicking a column header sorts the view only; Database keeps its own order.
    auto *sortModel = new QSortFilterProxyModel(this);
    sortModel->setSourceModel(model);
    sortModel->setSortRole(sortRole);
    sortModel->setSortCaseSensitivity(Qt::CaseInsensitive);

    auto *table = new QTableView;
    table->setModel(sortModel);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    // The table is still empty here, so this measures only the column titles:
    // widen a column where its title needs it, otherwise keep the default width.
    table->resizeColumnsToContents();
    for (int column = 0; column < model->columnCount(); ++column) {
        table->setColumnWidth(column, qMax(table->columnWidth(column),
                                           table->horizontalHeader()->defaultSectionSize()));
    }

    // Start in creation order (no sort column); a header click sorts by that column.
    table->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    table->setSortingEnabled(true);

    return table;
}

void MainWindow::showNavigationContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_navigationFilter->mapToSource(m_navigationTree->indexAt(pos));
    const QModelIndex signalsHeading = m_navigationModel->categoryIndex(NavigationModel::Signals);
    const QModelIndex messagesHeading = m_navigationModel->categoryIndex(NavigationModel::Messages);
    const int id = index.data(NavigationModel::ObjectIdRole).toInt();

    // Use the desktop theme's icons, falling back to Qt's built-in ones where no theme exists (e.g. Windows).
    const QIcon newIcon = QIcon::fromTheme("document-new", style()->standardIcon(QStyle::SP_FileIcon));
    const QIcon deleteIcon = QIcon::fromTheme("edit-delete", style()->standardIcon(QStyle::SP_TrashIcon));

    // Headings offer "New", items under them offer "Delete".
    QMenu menu(this);
    if (index == signalsHeading)
        menu.addAction(newIcon, "New", this, &MainWindow::createNewSignal);
    else if (index == messagesHeading)
        menu.addAction(newIcon, "New", this, &MainWindow::createNewMessage);
    else if (index.parent() == signalsHeading)
        menu.addAction(deleteIcon, "Delete", this, [this, id] { deleteSignal(id); });
    else if (index.parent() == messagesHeading)
        menu.addAction(deleteIcon, "Delete", this, [this, id] { deleteMessage(id); });

    if (!menu.isEmpty())
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

void MainWindow::onNavigationDoubleClicked(const QModelIndex &filterIndex)
{
    const QModelIndex index = m_navigationFilter->mapToSource(filterIndex);
    const int id = index.data(NavigationModel::ObjectIdRole).toInt();
    if (index.parent() == m_navigationModel->categoryIndex(NavigationModel::Signals))
        editSignal(id);
    else if (index.parent() == m_navigationModel->categoryIndex(NavigationModel::Messages))
        editMessage(id);
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

void MainWindow::createNewMessage()
{
    const QStringList existingNames = m_database->messageNames();

    // Pick the first free default name: New_Message_1, New_Message_2, ...
    Message newMessage;
    int number = 1;
    do {
        newMessage.name = QString("New_Message_%1").arg(number++);
    } while (existingNames.contains(newMessage.name));

    MessageDialog dialog(newMessage, existingNames, m_database->allSignals(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->addMessage(dialog.message());
}

void MainWindow::editMessage(int id)
{
    const Message *stored = m_database->findMessage(id);
    if (!stored)
        return;
    const Message message = *stored; // copy: the pointer is only valid until the next change

    QStringList otherNames = m_database->messageNames();
    otherNames.removeOne(message.name);

    MessageDialog dialog(message, otherNames, m_database->allSignals(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->updateMessage(dialog.message());
}

void MainWindow::deleteSignal(int id)
{
    const Signal *signal = m_database->findSignal(id);
    if (!signal)
        return;

    // A signal that a message carries cannot be deleted (Database refuses it too).
    const QStringList usedIn = m_database->messagesUsingSignal(id);
    if (!usedIn.isEmpty()) {
        QMessageBox::information(this, "Delete Signal",
                                 QString("The signal '%1' cannot be deleted because it is used in "
                                         "these messages:\n%2\n\nRemove it from them first.")
                                     .arg(signal->name, usedIn.join(", ")));
        return;
    }

    const QString question = QString("Delete the signal '%1'?").arg(signal->name);
    if (QMessageBox::question(this, "Delete Signal", question,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        m_database->removeSignal(id);
}

void MainWindow::deleteMessage(int id)
{
    const Message *message = m_database->findMessage(id);
    if (!message)
        return;

    // Its signals are not deleted: they stay under "Signals".
    const QString question = QString("Delete the message '%1'?").arg(message->name);
    if (QMessageBox::question(this, "Delete Message", question,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        m_database->removeMessage(id);
}

void MainWindow::onCurrentNavigationChanged(const QModelIndex &filterCurrent)
{
    const QModelIndex current = m_navigationFilter->mapToSource(filterCurrent);
    const QModelIndex signalsHeading = m_navigationModel->categoryIndex(NavigationModel::Signals);
    const QModelIndex messagesHeading = m_navigationModel->categoryIndex(NavigationModel::Messages);
    const int id = current.data(NavigationModel::ObjectIdRole).toInt();

    if (current == signalsHeading)
        showTable(m_signalTable);
    else if (current.parent() == signalsHeading)
        showSignalProperties(id);
    else if (current == messagesHeading)
        showTable(m_messageTable);
    else if (current.parent() == messagesHeading)
        showMessageProperties(id);
    else
        showEmptyPage();
}

void MainWindow::onSignalChanged(int id)
{
    if (id == m_shownSignalId)
        showSignalProperties(id);
    // The shown message lists its signals' names and lengths.
    if (m_shownMessageId != 0)
        showMessageProperties(m_shownMessageId);
}

void MainWindow::showEmptyPage()
{
    m_shownSignalId = 0;
    m_shownMessageId = 0;
    m_rightPane->setCurrentWidget(m_emptyPage);
}

void MainWindow::showSignalProperties(int id)
{
    const Signal *signal = m_database->findSignal(id);
    if (!signal) {
        showEmptyPage();
        return;
    }
    m_shownSignalId = id;
    m_shownMessageId = 0;
    m_signalProperties->showSignal(*signal);
    m_rightPane->setCurrentWidget(m_signalProperties);
}

void MainWindow::showMessageProperties(int id)
{
    const Message *message = m_database->findMessage(id);
    if (!message) {
        showEmptyPage();
        return;
    }
    m_shownSignalId = 0;
    m_shownMessageId = id;
    m_messageProperties->showMessage(*message, *m_database);
    m_rightPane->setCurrentWidget(m_messageProperties);
}

void MainWindow::showTable(QTableView *table)
{
    // The tables update themselves through their models; nothing to refresh by hand.
    m_shownSignalId = 0;
    m_shownMessageId = 0;
    m_rightPane->setCurrentWidget(table);
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
