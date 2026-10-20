#include "mainwindow.h"
#include "consistencychecker.h"
#include "consistencydialog.h"
#include "database.h"
#include "messagedialog.h"
#include "messagepropertieswidget.h"
#include "messagetablemodel.h"
#include "navigationfiltermodel.h"
#include "navigationmodel.h"
#include "networkdialog.h"
#include "networkpropertieswidget.h"
#include "nodedialog.h"
#include "nodepropertieswidget.h"
#include "nodetablemodel.h"
#include "signaldialog.h"
#include "signalpropertieswidget.h"
#include "signaltablemodel.h"
#include "valuetabledialog.h"
#include "valuetablepropertieswidget.h"
#include "valuetabletablemodel.h"
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
    connect(m_database, &Database::nodeAdded, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::nodeChanged, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::nodeRemoved, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::networkChanged, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::valueTableAdded, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::valueTableChanged, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::valueTableRemoved, this, [this] { setWindowModified(true); });
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

    QMenu *toolsMenu = menuBar()->addMenu("&Tools");
    toolsMenu->addAction(QIcon::fromTheme("tools-check-spelling"), "&Consistency Check...",
                         this, &MainWindow::checkConsistency, QKeySequence("F7"));
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
    m_networkProperties = new NetworkPropertiesWidget;
    m_signalProperties = new SignalPropertiesWidget;
    m_messageProperties = new MessagePropertiesWidget;
    m_nodeProperties = new NodePropertiesWidget;
    m_valueTableProperties = new ValueTablePropertiesWidget;
    m_signalTableModel = new SignalTableModel(m_database, this);
    m_messageTableModel = new MessageTableModel(m_database, this);
    m_nodeTableModel = new NodeTableModel(m_database, this);
    m_valueTableTableModel = new ValueTableTableModel(m_database, this);
    m_signalTable = createSortableTable(m_signalTableModel, SignalTableModel::SortRole);
    m_messageTable = createSortableTable(m_messageTableModel, MessageTableModel::SortRole);
    m_nodeTable = createSortableTable(m_nodeTableModel, NodeTableModel::SortRole);
    m_valueTableTable = createSortableTable(m_valueTableTableModel, ValueTableTableModel::SortRole);
    m_rightPane->addWidget(m_emptyPage);
    m_rightPane->addWidget(m_networkProperties);
    m_rightPane->addWidget(m_signalProperties);
    m_rightPane->addWidget(m_signalTable);
    m_rightPane->addWidget(m_messageProperties);
    m_rightPane->addWidget(m_messageTable);
    m_rightPane->addWidget(m_nodeProperties);
    m_rightPane->addWidget(m_nodeTable);
    m_rightPane->addWidget(m_valueTableProperties);
    m_rightPane->addWidget(m_valueTableTable);
    connect(m_navigationTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onCurrentNavigationChanged);

    // Keep the right pane up to date. The views show things from several places
    // (a message view lists signal and node names, a node view lists messages, ...),
    // so simply redraw the shown view after every change.
    connect(m_database, &Database::signalAdded, this, &MainWindow::refreshRightPane);
    connect(m_database, &Database::signalChanged, this, &MainWindow::refreshRightPane);
    connect(m_database, &Database::signalRemoved, this, &MainWindow::refreshRightPane);
    connect(m_database, &Database::messageAdded, this, &MainWindow::refreshRightPane);
    connect(m_database, &Database::messageChanged, this, &MainWindow::refreshRightPane);
    connect(m_database, &Database::messageRemoved, this, &MainWindow::refreshRightPane);
    connect(m_database, &Database::nodeAdded, this, &MainWindow::refreshRightPane);
    connect(m_database, &Database::nodeChanged, this, &MainWindow::refreshRightPane);
    connect(m_database, &Database::nodeRemoved, this, &MainWindow::refreshRightPane);
    connect(m_database, &Database::networkChanged, this, &MainWindow::refreshRightPane);
    connect(m_database, &Database::valueTableAdded, this, &MainWindow::refreshRightPane);
    connect(m_database, &Database::valueTableChanged, this, &MainWindow::refreshRightPane);
    connect(m_database, &Database::valueTableRemoved, this, &MainWindow::refreshRightPane);
    // After a file is opened nothing is selected any more: clear the right pane and show the signals.
    connect(m_database, &Database::wasReset, this, [this] {
        showView(View::Empty);
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
    if (!index.isValid())
        return;
    const int kind = index.data(NavigationModel::ItemKindRole).toInt();
    const int id = index.data(NavigationModel::ObjectIdRole).toInt();

    // Use the desktop theme's icons, falling back to Qt's built-in ones where no theme exists (e.g. Windows).
    const QIcon newIcon = QIcon::fromTheme("document-new", style()->standardIcon(QStyle::SP_FileIcon));
    const QIcon deleteIcon = QIcon::fromTheme("edit-delete", style()->standardIcon(QStyle::SP_TrashIcon));

    // Headings offer "New" ("Edit..." for Network), the items directly under them offer "Delete".
    QMenu menu(this);
    if (kind == NavigationModel::Heading) {
        if (index.row() == NavigationModel::Network)
            menu.addAction(QIcon::fromTheme("document-properties"), "Edit...", this, &MainWindow::editNetwork);
        else if (index.row() == NavigationModel::Signals)
            menu.addAction(newIcon, "New", this, &MainWindow::createNewSignal);
        else if (index.row() == NavigationModel::Messages)
            menu.addAction(newIcon, "New", this, &MainWindow::createNewMessage);
        else if (index.row() == NavigationModel::NetworkNodes)
            menu.addAction(newIcon, "New", this, &MainWindow::createNewNode);
        else if (index.row() == NavigationModel::ValueTables)
            menu.addAction(newIcon, "New", this, &MainWindow::createNewValueTable);
    } else if (kind == NavigationModel::SignalItem) {
        menu.addAction(deleteIcon, "Delete", this, [this, id] { deleteSignal(id); });
    } else if (kind == NavigationModel::MessageItem) {
        menu.addAction(deleteIcon, "Delete", this, [this, id] { deleteMessage(id); });
    } else if (kind == NavigationModel::NodeItem) {
        menu.addAction(deleteIcon, "Delete", this, [this, id] { deleteNode(id); });
    } else if (kind == NavigationModel::ValueTableItem) {
        menu.addAction(deleteIcon, "Delete", this, [this, id] { deleteValueTable(id); });
    }

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

    SignalDialog dialog(newSignal, existingNames, m_database->allValueTables(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // The tree updates itself: Database -> NavigationModel -> QTreeView.
    m_database->addSignal(dialog.signal());
}

void MainWindow::onNavigationDoubleClicked(const QModelIndex &filterIndex)
{
    const QModelIndex index = m_navigationFilter->mapToSource(filterIndex);
    const int id = index.data(NavigationModel::ObjectIdRole).toInt();
    switch (index.data(NavigationModel::ItemKindRole).toInt()) {
    case NavigationModel::Heading:
        if (index.row() == NavigationModel::Network)
            editNetwork();
        break;
    case NavigationModel::SignalItem:
        editSignal(id);
        break;
    case NavigationModel::MessageItem:
    case NavigationModel::NodeTxMessage:
    case NavigationModel::NodeRxMessage:
        editMessage(id);
        break;
    case NavigationModel::NodeItem:
        editNode(id);
        break;
    case NavigationModel::ValueTableItem:
        editValueTable(id);
        break;
    }
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

    SignalDialog dialog(signal, otherNames, m_database->allValueTables(), this);
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

    MessageDialog dialog(newMessage, existingNames, m_database->allSignals(), m_database->allNodes(),
                         m_database->network(), this);
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

    MessageDialog dialog(message, otherNames, m_database->allSignals(), m_database->allNodes(),
                         m_database->network(), this);
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

void MainWindow::createNewNode()
{
    const QStringList existingNames = m_database->nodeNames();

    // Pick the first free default name: New_Node_1, New_Node_2, ...
    Node newNode;
    int number = 1;
    do {
        newNode.name = QString("New_Node_%1").arg(number++);
    } while (existingNames.contains(newNode.name));

    NodeDialog dialog(newNode, existingNames, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->addNode(dialog.node());
}

void MainWindow::editNode(int id)
{
    const Node *stored = m_database->findNode(id);
    if (!stored)
        return;
    const Node node = *stored; // copy: the pointer is only valid until the next change

    QStringList otherNames = m_database->nodeNames();
    otherNames.removeOne(node.name);

    NodeDialog dialog(node, otherNames, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->updateNode(dialog.node());
}

void MainWindow::deleteNode(int id)
{
    const Node *node = m_database->findNode(id);
    if (!node)
        return;

    // A node that transmits or receives a message cannot be deleted (Database refuses it too).
    QStringList usedIn;
    for (int messageId : m_database->messagesTransmittedBy(id))
        usedIn << m_database->findMessage(messageId)->name + " (Tx)";
    for (int messageId : m_database->messagesReceivedBy(id))
        usedIn << m_database->findMessage(messageId)->name + " (Rx)";
    if (!usedIn.isEmpty()) {
        QMessageBox::information(this, "Delete Network Node",
                                 QString("The network node '%1' cannot be deleted because it "
                                         "transmits or receives these messages:\n%2\n\n"
                                         "Remove it from them first.")
                                     .arg(node->name, usedIn.join(", ")));
        return;
    }

    const QString question = QString("Delete the network node '%1'?").arg(node->name);
    if (QMessageBox::question(this, "Delete Network Node", question,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        m_database->removeNode(id);
}

void MainWindow::editNetwork()
{
    NetworkDialog dialog(m_database->network(), m_database->allMessages(), this);
    if (dialog.exec() == QDialog::Accepted)
        m_database->setNetwork(dialog.network());
}

void MainWindow::createNewValueTable()
{
    const QStringList existingNames = m_database->valueTableNames();

    // Pick the first free default name: New_Value_Table_1, New_Value_Table_2, ...
    ValueTable newValueTable;
    int number = 1;
    do {
        newValueTable.name = QString("New_Value_Table_%1").arg(number++);
    } while (existingNames.contains(newValueTable.name));

    ValueTableDialog dialog(newValueTable, existingNames, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->addValueTable(dialog.valueTable());
}

void MainWindow::editValueTable(int id)
{
    const ValueTable *stored = m_database->findValueTable(id);
    if (!stored)
        return;
    const ValueTable valueTable = *stored; // copy: the pointer is only valid until the next change

    QStringList otherNames = m_database->valueTableNames();
    otherNames.removeOne(valueTable.name);

    ValueTableDialog dialog(valueTable, otherNames, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->updateValueTable(dialog.valueTable());
}

void MainWindow::deleteValueTable(int id)
{
    const ValueTable *valueTable = m_database->findValueTable(id);
    if (!valueTable)
        return;

    // A value table that a signal uses cannot be deleted (Database refuses it too).
    const QStringList usedBy = m_database->signalsUsingValueTable(id);
    if (!usedBy.isEmpty()) {
        QMessageBox::information(this, "Delete Value Table",
                                 QString("The value table '%1' cannot be deleted because these "
                                         "signals use it:\n%2\n\nChoose another value table for them first.")
                                     .arg(valueTable->name, usedBy.join(", ")));
        return;
    }

    const QString question = QString("Delete the value table '%1'?").arg(valueTable->name);
    if (QMessageBox::question(this, "Delete Value Table", question,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        m_database->removeValueTable(id);
}

void MainWindow::checkConsistency()
{
    ConsistencyDialog dialog(ConsistencyChecker::check(*m_database), this);
    dialog.exec();
}

void MainWindow::onCurrentNavigationChanged(const QModelIndex &filterCurrent)
{
    const QModelIndex current = m_navigationFilter->mapToSource(filterCurrent);
    if (!current.isValid()) {
        showView(View::Empty);
        return;
    }
    const int id = current.data(NavigationModel::ObjectIdRole).toInt();

    switch (current.data(NavigationModel::ItemKindRole).toInt()) {
    case NavigationModel::Heading:
        switch (current.row()) {
        case NavigationModel::Network:      showView(View::Network); break;
        case NavigationModel::NetworkNodes: showTable(m_nodeTable); break;
        case NavigationModel::Messages:     showTable(m_messageTable); break;
        case NavigationModel::Signals:      showTable(m_signalTable); break;
        case NavigationModel::ValueTables:  showTable(m_valueTableTable); break;
        }
        break;
    case NavigationModel::SignalItem:
        showView(View::Signal, id);
        break;
    case NavigationModel::MessageItem:
    case NavigationModel::NodeTxMessage:
    case NavigationModel::NodeRxMessage:
        showView(View::Message, id);
        break;
    case NavigationModel::NodeItem:
    case NavigationModel::NodeTxFolder: // the node view lists its Tx and Rx messages
    case NavigationModel::NodeRxFolder:
        showView(View::Node, id);
        break;
    case NavigationModel::ValueTableItem:
        showView(View::ValueTable, id);
        break;
    }
}

void MainWindow::showView(View view, int id)
{
    m_shownView = view;
    m_shownId = id;
    refreshRightPane();
}

void MainWindow::showTable(QTableView *table)
{
    // The tables update themselves through their models; nothing to redraw by hand.
    m_shownView = View::Table;
    m_shownId = 0;
    m_rightPane->setCurrentWidget(table);
}

void MainWindow::refreshRightPane()
{
    switch (m_shownView) {
    case View::Table:
        return;
    case View::Network:
        m_networkProperties->showNetwork(*m_database, m_currentFilePath);
        m_rightPane->setCurrentWidget(m_networkProperties);
        return;
    case View::Signal:
        if (const Signal *signal = m_database->findSignal(m_shownId)) {
            m_signalProperties->showSignal(*signal, *m_database);
            m_rightPane->setCurrentWidget(m_signalProperties);
            return;
        }
        break;
    case View::Message:
        if (const Message *message = m_database->findMessage(m_shownId)) {
            m_messageProperties->showMessage(*message, *m_database);
            m_rightPane->setCurrentWidget(m_messageProperties);
            return;
        }
        break;
    case View::Node:
        if (const Node *node = m_database->findNode(m_shownId)) {
            m_nodeProperties->showNode(*node, *m_database);
            m_rightPane->setCurrentWidget(m_nodeProperties);
            return;
        }
        break;
    case View::ValueTable:
        if (const ValueTable *valueTable = m_database->findValueTable(m_shownId)) {
            m_valueTableProperties->showValueTable(*valueTable, *m_database);
            m_rightPane->setCurrentWidget(m_valueTableProperties);
            return;
        }
        break;
    case View::Empty:
        break;
    }

    // Nothing to show, or what was shown has been deleted.
    m_shownView = View::Empty;
    m_shownId = 0;
    m_rightPane->setCurrentWidget(m_emptyPage);
}

namespace {
// FrameForge files are XML inside, but get their own extension.
const QString FileExtension = "ffxml";
const QString FileFilter = "FrameForge files (*.ffxml);;All files (*)";
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
    // Always save with our extension: "engine" and "engine.txt" become
    // "engine.ffxml" and "engine.txt.ffxml".
    if (QFileInfo(filePath).suffix().compare(FileExtension, Qt::CaseInsensitive) != 0)
        filePath += "." + FileExtension;

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
    refreshRightPane(); // the Network view shows the file path
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
