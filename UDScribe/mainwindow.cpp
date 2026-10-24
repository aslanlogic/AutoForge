#include "mainwindow.h"
#include "navigationmodel.h"
#include "networkdialog.h"
#include "networkpropertieswidget.h"
#include "securityleveldialog.h"
#include "securitylevelpropertieswidget.h"
#include "securityleveltablemodel.h"
#include "sessiondialog.h"
#include "sessionpropertieswidget.h"
#include "sessiontablemodel.h"
#include "usccore/database.h"
#include "usccore/xmlstorage.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTableView>
#include <QTreeView>

namespace {

// The sessionType sub-functions of all sessions except the one with this id.
QVector<quint8> sessionTypesUsedByOthers(const Database &database, int id)
{
    QVector<quint8> sessionTypes;
    for (const Session &session : database.allSessions()) {
        if (session.id != id)
            sessionTypes.append(session.sessionType);
    }
    return sessionTypes;
}

// The requestSeed sub-functions of all security levels except the one with this id.
QVector<quint8> requestSeedsUsedByOthers(const Database &database, int id)
{
    QVector<quint8> requestSeeds;
    for (const SecurityLevel &securityLevel : database.allSecurityLevels()) {
        if (securityLevel.id != id)
            requestSeeds.append(securityLevel.requestSeed);
    }
    return requestSeeds;
}

} // namespace

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
    connect(m_database, &Database::networkChanged, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::sessionAdded, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::sessionChanged, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::sessionRemoved, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::securityLevelAdded, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::securityLevelChanged, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::securityLevelRemoved, this, [this] { setWindowModified(true); });
}

void MainWindow::setupMenus()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(QIcon::fromTheme("document-new"), "&New",
                        this, &MainWindow::newFile, QKeySequence::New);
    fileMenu->addAction(QIcon::fromTheme("document-open"), "&Open...",
                        this, &MainWindow::openFile, QKeySequence::Open);
    fileMenu->addAction(QIcon::fromTheme("document-save"), "&Save",
                        this, &MainWindow::saveFile, QKeySequence::Save);
    fileMenu->addAction(QIcon::fromTheme("document-save-as"), "Save &As...",
                        this, &MainWindow::saveFileAs, QKeySequence("Ctrl+Shift+S"));
}

void MainWindow::setupLayout()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Left pane: the services tree. QTreeView scrolls on its own when content overflows.
    m_navigationTree = new QTreeView(m_splitter);
    m_navigationTree->setHeaderHidden(true);
    m_navigationTree->setModel(m_navigationModel);

    // Show newly added items by expanding their service.
    connect(m_navigationModel, &NavigationModel::rowsInserted,
            m_navigationTree, &QTreeView::expand);
    // And show everything of a newly opened file. The model resets itself on the
    // same signal first, because it was connected first.
    connect(m_database, &Database::wasReset, m_navigationTree, &QTreeView::expandAll);

    m_navigationTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_navigationTree, &QTreeView::customContextMenuRequested,
            this, &MainWindow::showNavigationContextMenu);
    connect(m_navigationTree, &QTreeView::doubleClicked,
            this, &MainWindow::onNavigationDoubleClicked);

    // Right pane: shows what is clicked in the tree.
    m_rightPane = new QStackedWidget(m_splitter);
    m_emptyPage = new QWidget;
    m_sessionTableModel = new SessionTableModel(m_database, this);
    m_sessionTable = createSortableTable(m_sessionTableModel, SessionTableModel::SortRole);
    m_sessionProperties = new SessionPropertiesWidget;
    m_securityLevelTableModel = new SecurityLevelTableModel(m_database, this);
    m_securityLevelTable = createSortableTable(m_securityLevelTableModel, SecurityLevelTableModel::SortRole);
    m_networkProperties = new NetworkPropertiesWidget;
    m_securityLevelProperties = new SecurityLevelPropertiesWidget;
    m_rightPane->addWidget(m_emptyPage);
    m_rightPane->addWidget(m_networkProperties);
    m_rightPane->addWidget(m_sessionTable);
    m_rightPane->addWidget(m_sessionProperties);
    m_rightPane->addWidget(m_securityLevelTable);
    m_rightPane->addWidget(m_securityLevelProperties);
    connect(m_navigationTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onCurrentNavigationChanged);

    // Keep the shown network and security level up to date. The table updates itself through its model.
    connect(m_database, &Database::networkChanged, this, &MainWindow::refreshNetworkProperties);
    connect(m_database, &Database::sessionChanged, this, &MainWindow::refreshSessionProperties);
    connect(m_database, &Database::sessionRemoved, this, &MainWindow::refreshSessionProperties);
    connect(m_database, &Database::securityLevelChanged, this, &MainWindow::refreshSecurityLevelProperties);
    connect(m_database, &Database::sessionChanged, this, &MainWindow::refreshSecurityLevelProperties); // session names
    connect(m_database, &Database::securityLevelRemoved, this, &MainWindow::refreshSecurityLevelProperties);
    // New and Open renumber all ids and leave nothing selected in the tree: start empty.
    connect(m_database, &Database::wasReset, this, [this] {
        m_shownSessionId = 0;
        m_shownSecurityLevelId = 0;
        m_rightPane->setCurrentWidget(m_emptyPage);
    });

    m_splitter->addWidget(m_navigationTree);
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
    const QModelIndex index = m_navigationTree->indexAt(pos);
    if (!index.isValid())
        return;
    const int kind = index.data(NavigationModel::ItemKindRole).toInt();
    const int id = index.data(NavigationModel::ObjectIdRole).toInt();

    // Use the desktop theme's icons, falling back to Qt's built-in ones where no theme exists (e.g. Windows).
    const QIcon newIcon = QIcon::fromTheme("document-new", style()->standardIcon(QStyle::SP_FileIcon));
    const QIcon deleteIcon = QIcon::fromTheme("edit-delete", style()->standardIcon(QStyle::SP_TrashIcon));

    // Service headings offer "Add...", the items under them offer "Delete".
    QMenu menu(this);
    if (kind == NavigationModel::Heading) {
        if (index.row() == NavigationModel::DiagnosticSessionControl)
            menu.addAction(newIcon, "Add...", this, &MainWindow::createNewSession);
        else if (index.row() == NavigationModel::SecurityAccess)
            menu.addAction(newIcon, "Add...", this, &MainWindow::createNewSecurityLevel);
    } else if (kind == NavigationModel::SessionItem) {
        menu.addAction(deleteIcon, "Delete", this, [this, id] { deleteSession(id); });
    } else if (kind == NavigationModel::SecurityLevelItem) {
        menu.addAction(deleteIcon, "Delete", this, [this, id] { deleteSecurityLevel(id); });
    }

    if (!menu.isEmpty())
        menu.exec(m_navigationTree->viewport()->mapToGlobal(pos));
}

void MainWindow::onNavigationDoubleClicked(const QModelIndex &index)
{
    const int id = index.data(NavigationModel::ObjectIdRole).toInt();
    switch (index.data(NavigationModel::ItemKindRole).toInt()) {
    case NavigationModel::Heading:
        if (index.row() == NavigationModel::Network)
            editNetwork();
        break;
    case NavigationModel::SessionItem:
        editSession(id);
        break;
    case NavigationModel::SecurityLevelItem:
        editSecurityLevel(id);
        break;
    }
}

void MainWindow::onCurrentNavigationChanged(const QModelIndex &current)
{
    // A service shows the table of all its items, an item shows its own properties.
    m_shownSessionId = 0;
    m_shownSecurityLevelId = 0;
    switch (current.data(NavigationModel::ItemKindRole).toInt()) {
    case NavigationModel::Heading:
        if (current.row() == NavigationModel::Network) {
            m_rightPane->setCurrentWidget(m_networkProperties);
            refreshNetworkProperties();
            return;
        }
        if (current.row() == NavigationModel::DiagnosticSessionControl) {
            m_rightPane->setCurrentWidget(m_sessionTable);
            return;
        }
        if (current.row() == NavigationModel::SecurityAccess) {
            m_rightPane->setCurrentWidget(m_securityLevelTable);
            return;
        }
        break;
    case NavigationModel::SessionItem:
        m_shownSessionId = current.data(NavigationModel::ObjectIdRole).toInt();
        refreshSessionProperties();
        return;
    case NavigationModel::SecurityLevelItem:
        m_shownSecurityLevelId = current.data(NavigationModel::ObjectIdRole).toInt();
        refreshSecurityLevelProperties();
        return;
    }
    m_rightPane->setCurrentWidget(m_emptyPage);
}

void MainWindow::refreshSessionProperties()
{
    if (m_shownSessionId == 0)
        return; // no session is shown

    if (const Session *session = m_database->findSession(m_shownSessionId)) {
        m_sessionProperties->showSession(*session);
        m_rightPane->setCurrentWidget(m_sessionProperties);
    } else {
        m_shownSessionId = 0;
        m_rightPane->setCurrentWidget(m_emptyPage);
    }
}

void MainWindow::refreshSecurityLevelProperties()
{
    if (m_shownSecurityLevelId == 0)
        return; // no security level is shown

    if (const SecurityLevel *securityLevel = m_database->findSecurityLevel(m_shownSecurityLevelId)) {
        m_securityLevelProperties->showSecurityLevel(*securityLevel, *m_database);
        m_rightPane->setCurrentWidget(m_securityLevelProperties);
    } else {
        m_shownSecurityLevelId = 0;
        m_rightPane->setCurrentWidget(m_emptyPage);
    }
}

void MainWindow::refreshNetworkProperties()
{
    if (m_rightPane->currentWidget() == m_networkProperties)
        m_networkProperties->showNetwork(*m_database, m_currentFilePath);
}

void MainWindow::editNetwork()
{
    NetworkDialog dialog(m_database->network(), this);
    if (dialog.exec() == QDialog::Accepted)
        m_database->setNetwork(dialog.network());
}

void MainWindow::createNewSession()
{
    const QStringList existingNames = m_database->sessionNames();
    const QVector<quint8> usedSessionTypes = sessionTypesUsedByOthers(*m_database, 0);

    // Pick the first free default name: New_Session_1, New_Session_2, ...
    Session newSession;
    int number = 1;
    do {
        newSession.name = QString("New_Session_%1").arg(number++);
    } while (existingNames.contains(newSession.name));

    // And the first free sessionType sub-function after the default one. If none
    // is free, the dialog says so.
    for (int value = 0; value <= 0xFF; ++value) {
        const quint8 sessionType = quint8(value);
        if (isValidSessionType(sessionType) && sessionType != DefaultSessionType
            && !usedSessionTypes.contains(sessionType)) {
            newSession.sessionType = sessionType;
            break;
        }
    }

    SessionDialog dialog(newSession, existingNames, usedSessionTypes, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->addSession(dialog.session());
}

void MainWindow::editSession(int id)
{
    const Session *stored = m_database->findSession(id);
    if (!stored)
        return;
    const Session session = *stored; // copy: the pointer is only valid until the next change

    QStringList otherNames = m_database->sessionNames();
    otherNames.removeOne(session.name);

    SessionDialog dialog(session, otherNames, sessionTypesUsedByOthers(*m_database, id), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->updateSession(dialog.session());
}

void MainWindow::deleteSession(int id)
{
    const Session *session = m_database->findSession(id);
    if (!session)
        return;
    const QString label = subFunctionToText(session->sessionType) + ' ' + session->name;

    // The default session and a session that a security level is allowed in
    // cannot be deleted (Database refuses it too).
    if (session->sessionType == DefaultSessionType) {
        QMessageBox::information(this, "Delete Session",
                                 QString("The session '%1' cannot be deleted: every ECU has the "
                                         "default session.").arg(label));
        return;
    }
    const QStringList usedBy = m_database->securityLevelsUsingSession(id);
    if (!usedBy.isEmpty()) {
        QMessageBox::information(this, "Delete Session",
                                 QString("The session '%1' cannot be deleted because these security "
                                         "levels are allowed in it:\n%2\n\nRemove it from them first.")
                                     .arg(label, usedBy.join(", ")));
        return;
    }

    const QString question = QString("Delete the session '%1'?").arg(label);
    if (QMessageBox::question(this, "Delete Session", question,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        m_database->removeSession(id);
}

void MainWindow::createNewSecurityLevel()
{
    const QStringList existingNames = m_database->securityLevelNames();
    const QVector<quint8> usedRequestSeeds = requestSeedsUsedByOthers(*m_database, 0);

    // Pick the first free default name: New_SecurityLevel_1, New_SecurityLevel_2, ...
    SecurityLevel newSecurityLevel;
    int number = 1;
    do {
        newSecurityLevel.name = QString("New_SecurityLevel_%1").arg(number++);
    } while (existingNames.contains(newSecurityLevel.name));

    // And the first free requestSeed sub-function. If none is free, the dialog says so.
    for (int value = 0; value <= 0xFF; ++value) {
        const quint8 requestSeed = quint8(value);
        if (isValidRequestSeed(requestSeed) && !usedRequestSeeds.contains(requestSeed)) {
            newSecurityLevel.requestSeed = requestSeed;
            break;
        }
    }

    SecurityLevelDialog dialog(newSecurityLevel, existingNames, usedRequestSeeds, m_database->allSessions(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->addSecurityLevel(dialog.securityLevel());
}

void MainWindow::editSecurityLevel(int id)
{
    const SecurityLevel *stored = m_database->findSecurityLevel(id);
    if (!stored)
        return;
    const SecurityLevel securityLevel = *stored; // copy: the pointer is only valid until the next change

    QStringList otherNames = m_database->securityLevelNames();
    otherNames.removeOne(securityLevel.name);

    SecurityLevelDialog dialog(securityLevel, otherNames, requestSeedsUsedByOthers(*m_database, id),
                               m_database->allSessions(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->updateSecurityLevel(dialog.securityLevel());
}

void MainWindow::deleteSecurityLevel(int id)
{
    const SecurityLevel *securityLevel = m_database->findSecurityLevel(id);
    if (!securityLevel)
        return;

    const QString question = QString("Delete the security level '%1 %2'?")
                                 .arg(subFunctionToText(securityLevel->requestSeed), securityLevel->name);
    if (QMessageBox::question(this, "Delete Security Level", question,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        m_database->removeSecurityLevel(id);
}

namespace {
// UDScribe files are XML inside, but get their own extension.
const QString FileExtension = "uscxml";
const QString FileFilter = "UDScribe files (*.uscxml);;All files (*)";
}

void MainWindow::newFile()
{
    if (!maybeSave())
        return;

    m_database->replaceAll(Network(), {}, {});
    setCurrentFilePath(QString());
    setWindowModified(false);
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
    // "engine.uscxml" and "engine.txt.uscxml".
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
    setWindowTitle(QString("%1[*] - UDScribe").arg(fileName));
    refreshNetworkProperties(); // the Network view shows the file path
}

bool MainWindow::maybeSave()
{
    if (!isWindowModified())
        return true;

    const auto answer = QMessageBox::warning(
        this, "UDScribe", "The document has unsaved changes.\nDo you want to save them?",
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
