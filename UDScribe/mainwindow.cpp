#include "mainwindow.h"
#include "dataidentifierdialog.h"
#include "dataidentifierpropertieswidget.h"
#include "dataidentifiertablemodel.h"
#include "accessdialog.h"
#include "datatypedialog.h"
#include "datatypepropertieswidget.h"
#include "datatypetablemodel.h"
#include "navigationmodel.h"
#include "networkdialog.h"
#include "networkpropertieswidget.h"
#include "resettypedialog.h"
#include "resettypepropertieswidget.h"
#include "resettypetablemodel.h"
#include "securityleveldialog.h"
#include "securitylevelpropertieswidget.h"
#include "securityleveltablemodel.h"
#include "sessiondialog.h"
#include "sessionpropertieswidget.h"
#include "sessiontablemodel.h"
#include "valuetabledialog.h"
#include "valuetablepropertieswidget.h"
#include "valuetabletablemodel.h"
#include "usccore/database.h"
#include "usccore/xmlstorage.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
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

// The rows of a DataIdentifierTableModel whose DID can be read (0x22 ReadDataByIdentifier)
// or written (0x2E WriteDataByIdentifier). It filters again whenever a DID changes
// (QSortFilterProxyModel's dynamic filtering).
class DataIdentifierAccessFilter : public QSortFilterProxyModel
{
public:
    DataIdentifierAccessFilter(const Database *database, DidAccess access, QObject *parent)
        : QSortFilterProxyModel(parent), m_database(database), m_access(access) {}

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &) const override
    {
        return hasAccess(m_database->allDataIdentifiers().at(sourceRow), m_access);
    }

private:
    const Database *m_database = nullptr;
    DidAccess m_access;
};

// "0x22" / "read" etc.: the words for an access in messages.
QString serviceOf(DidAccess access) { return access == DidAccess::Read ? "0x22" : "0x2E"; }
QString verbOf(DidAccess access) { return access == DidAccess::Read ? "read" : "written"; }
QString adjectiveOf(DidAccess access) { return access == DidAccess::Read ? "readable" : "writable"; }

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

// The DID numbers of all DIDs except the one with this id.
QVector<quint16> didsUsedByOthers(const Database &database, int id)
{
    QVector<quint16> dids;
    for (const DataIdentifier &dataIdentifier : database.allDataIdentifiers()) {
        if (dataIdentifier.id != id)
            dids.append(dataIdentifier.did);
    }
    return dids;
}

// The resetType sub-functions of all reset types except the one with this id.
QVector<quint8> resetTypesUsedByOthers(const Database &database, int id)
{
    QVector<quint8> resetTypes;
    for (const ResetType &resetType : database.allResetTypes()) {
        if (resetType.id != id)
            resetTypes.append(resetType.resetType);
    }
    return resetTypes;
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
    connect(m_database, &Database::resetTypeAdded, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::resetTypeChanged, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::resetTypeRemoved, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::valueTableAdded, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::valueTableChanged, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::valueTableRemoved, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::dataIdentifierAdded, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::dataIdentifierChanged, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::dataIdentifierRemoved, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::dataTypeAdded, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::dataTypeChanged, this, [this] { setWindowModified(true); });
    connect(m_database, &Database::dataTypeRemoved, this, [this] { setWindowModified(true); });
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
    m_resetTypeTableModel = new ResetTypeTableModel(m_database, this);
    m_resetTypeTable = createSortableTable(m_resetTypeTableModel, ResetTypeTableModel::SortRole);
    m_resetTypeProperties = new ResetTypePropertiesWidget;
    m_securityLevelTableModel = new SecurityLevelTableModel(m_database, this);
    m_securityLevelTable = createSortableTable(m_securityLevelTableModel, SecurityLevelTableModel::SortRole);
    m_networkProperties = new NetworkPropertiesWidget;
    m_securityLevelProperties = new SecurityLevelPropertiesWidget;
    m_valueTableTableModel = new ValueTableTableModel(m_database, this);
    m_valueTableTable = createSortableTable(m_valueTableTableModel, ValueTableTableModel::SortRole);
    m_valueTableProperties = new ValueTablePropertiesWidget;
    m_dataTypeTableModel = new DataTypeTableModel(m_database, this);
    m_dataTypeTable = createSortableTable(m_dataTypeTableModel, DataTypeTableModel::SortRole);
    m_dataTypeProperties = new DataTypePropertiesWidget;
    m_dataIdentifierTableModel = new DataIdentifierTableModel(m_database, this);
    m_dataIdentifierTable = createSortableTable(m_dataIdentifierTableModel, DataIdentifierTableModel::SortRole);
    m_dataIdentifierProperties = new DataIdentifierPropertiesWidget;
    auto *readableFilter = new DataIdentifierAccessFilter(m_database, DidAccess::Read, this);
    readableFilter->setSourceModel(m_dataIdentifierTableModel);
    m_readDataIdentifierTable = createSortableTable(readableFilter, DataIdentifierTableModel::SortRole);
    auto *writableFilter = new DataIdentifierAccessFilter(m_database, DidAccess::Write, this);
    writableFilter->setSourceModel(m_dataIdentifierTableModel);
    m_writeDataIdentifierTable = createSortableTable(writableFilter, DataIdentifierTableModel::SortRole);
    m_rightPane->addWidget(m_emptyPage);
    m_rightPane->addWidget(m_networkProperties);
    m_rightPane->addWidget(m_sessionTable);
    m_rightPane->addWidget(m_sessionProperties);
    m_rightPane->addWidget(m_resetTypeTable);
    m_rightPane->addWidget(m_resetTypeProperties);
    m_rightPane->addWidget(m_securityLevelTable);
    m_rightPane->addWidget(m_securityLevelProperties);
    m_rightPane->addWidget(m_valueTableTable);
    m_rightPane->addWidget(m_valueTableProperties);
    m_rightPane->addWidget(m_dataTypeTable);
    m_rightPane->addWidget(m_dataTypeProperties);
    m_rightPane->addWidget(m_dataIdentifierTable);
    m_rightPane->addWidget(m_readDataIdentifierTable);
    m_rightPane->addWidget(m_writeDataIdentifierTable);
    m_rightPane->addWidget(m_dataIdentifierProperties);
    connect(m_navigationTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onCurrentNavigationChanged);

    // Keep the shown network and security level up to date. The table updates itself through its model.
    connect(m_database, &Database::networkChanged, this, &MainWindow::refreshNetworkProperties);
    connect(m_database, &Database::sessionChanged, this, &MainWindow::refreshSessionProperties);
    connect(m_database, &Database::sessionRemoved, this, &MainWindow::refreshSessionProperties);
    connect(m_database, &Database::resetTypeChanged, this, &MainWindow::refreshResetTypeProperties);
    connect(m_database, &Database::resetTypeRemoved, this, &MainWindow::refreshResetTypeProperties);
    connect(m_database, &Database::sessionChanged, this, &MainWindow::refreshResetTypeProperties);       // names
    connect(m_database, &Database::securityLevelChanged, this, &MainWindow::refreshResetTypeProperties); // names
    connect(m_database, &Database::securityLevelChanged, this, &MainWindow::refreshSecurityLevelProperties);
    connect(m_database, &Database::valueTableChanged, this, &MainWindow::refreshValueTableProperties);
    connect(m_database, &Database::valueTableRemoved, this, &MainWindow::refreshValueTableProperties);
    // A value table's page lists its data types; a data type's page shows its value table's name.
    connect(m_database, &Database::dataTypeAdded, this, &MainWindow::refreshValueTableProperties);
    connect(m_database, &Database::dataTypeChanged, this, &MainWindow::refreshValueTableProperties);
    connect(m_database, &Database::dataTypeRemoved, this, &MainWindow::refreshValueTableProperties);
    connect(m_database, &Database::dataTypeChanged, this, &MainWindow::refreshDataTypeProperties);
    connect(m_database, &Database::dataTypeRemoved, this, &MainWindow::refreshDataTypeProperties);
    connect(m_database, &Database::valueTableChanged, this, &MainWindow::refreshDataTypeProperties);
    // A data type's page lists its DIDs; a DID's page shows names of data types, sessions and levels.
    for (auto change : {&Database::dataIdentifierAdded, &Database::dataIdentifierChanged, &Database::dataIdentifierRemoved})
        connect(m_database, change, this, &MainWindow::refreshDataTypeProperties);
    for (auto change : {&Database::dataIdentifierChanged, &Database::dataIdentifierRemoved, &Database::dataTypeChanged,
                        &Database::sessionChanged, &Database::securityLevelChanged})
        connect(m_database, change, this, &MainWindow::refreshDataIdentifierProperties);
    connect(m_database, &Database::sessionChanged, this, &MainWindow::refreshSecurityLevelProperties); // session names
    connect(m_database, &Database::securityLevelRemoved, this, &MainWindow::refreshSecurityLevelProperties);
    // New and Open renumber all ids and leave nothing selected in the tree: start empty.
    connect(m_database, &Database::wasReset, this, [this] {
        m_shownSessionId = 0;
        m_shownResetTypeId = 0;
        m_shownSecurityLevelId = 0;
        m_shownValueTableId = 0;
        m_shownDataTypeId = 0;
        m_shownDataIdentifierId = 0;
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
        else if (index.row() == NavigationModel::EcuReset)
            menu.addAction(newIcon, "Add...", this, &MainWindow::createNewResetType);
        else if (index.row() == NavigationModel::ReadDataByIdentifier)
            menu.addAction(newIcon, "Add...", this, [this] { addAccessDataIdentifier(DidAccess::Read); });
        else if (index.row() == NavigationModel::WriteDataByIdentifier)
            menu.addAction(newIcon, "Add...", this, [this] { addAccessDataIdentifier(DidAccess::Write); });
        else if (index.row() == NavigationModel::SecurityAccess)
            menu.addAction(newIcon, "Add...", this, &MainWindow::createNewSecurityLevel);
        else if (index.row() == NavigationModel::ValueTables)
            menu.addAction(newIcon, "Add...", this, &MainWindow::createNewValueTable);
        else if (index.row() == NavigationModel::DataTypes)
            menu.addAction(newIcon, "Add...", this, &MainWindow::createNewDataType);
        else if (index.row() == NavigationModel::DataIdentifiers)
            menu.addAction(newIcon, "Add...", this, &MainWindow::createNewDataIdentifier);
    } else if (kind == NavigationModel::SessionItem) {
        menu.addAction(deleteIcon, "Delete", this, [this, id] { deleteSession(id); });
    } else if (kind == NavigationModel::ResetTypeItem) {
        menu.addAction(deleteIcon, "Delete", this, [this, id] { deleteResetType(id); });
    } else if (kind == NavigationModel::SecurityLevelItem) {
        menu.addAction(deleteIcon, "Delete", this, [this, id] { deleteSecurityLevel(id); });
    } else if (kind == NavigationModel::ValueTableItem) {
        menu.addAction(deleteIcon, "Delete", this, [this, id] { deleteValueTable(id); });
    } else if (kind == NavigationModel::DataTypeItem) {
        menu.addAction(deleteIcon, "Delete", this, [this, id] { deleteDataType(id); });
    } else if (kind == NavigationModel::DataIdentifierItem) {
        menu.addAction(deleteIcon, "Delete", this, [this, id] { deleteDataIdentifier(id); });
    } else if (kind == NavigationModel::ReadDataIdentifierItem) {
        menu.addAction(deleteIcon, "Remove", this, [this, id] { removeAccess(id, DidAccess::Read); });
    } else if (kind == NavigationModel::WriteDataIdentifierItem) {
        menu.addAction(deleteIcon, "Remove", this, [this, id] { removeAccess(id, DidAccess::Write); });
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
    case NavigationModel::ResetTypeItem:
        editResetType(id);
        break;
    case NavigationModel::SecurityLevelItem:
        editSecurityLevel(id);
        break;
    case NavigationModel::ValueTableItem:
        editValueTable(id);
        break;
    case NavigationModel::DataTypeItem:
        editDataType(id);
        break;
    case NavigationModel::DataIdentifierItem:
        editDataIdentifier(id);
        break;
    case NavigationModel::ReadDataIdentifierItem:
        editAccess(id, DidAccess::Read);
        break;
    case NavigationModel::WriteDataIdentifierItem:
        editAccess(id, DidAccess::Write);
        break;
    }
}

void MainWindow::onCurrentNavigationChanged(const QModelIndex &current)
{
    // A service shows the table of all its items, an item shows its own properties.
    m_shownSessionId = 0;
    m_shownResetTypeId = 0;
    m_shownSecurityLevelId = 0;
    m_shownValueTableId = 0;
    m_shownDataTypeId = 0;
    m_shownDataIdentifierId = 0;
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
        if (current.row() == NavigationModel::EcuReset) {
            m_rightPane->setCurrentWidget(m_resetTypeTable);
            return;
        }
        if (current.row() == NavigationModel::ReadDataByIdentifier) {
            m_rightPane->setCurrentWidget(m_readDataIdentifierTable);
            return;
        }
        if (current.row() == NavigationModel::WriteDataByIdentifier) {
            m_rightPane->setCurrentWidget(m_writeDataIdentifierTable);
            return;
        }
        if (current.row() == NavigationModel::SecurityAccess) {
            m_rightPane->setCurrentWidget(m_securityLevelTable);
            return;
        }
        if (current.row() == NavigationModel::ValueTables) {
            m_rightPane->setCurrentWidget(m_valueTableTable);
            return;
        }
        if (current.row() == NavigationModel::DataTypes) {
            m_rightPane->setCurrentWidget(m_dataTypeTable);
            return;
        }
        if (current.row() == NavigationModel::DataIdentifiers) {
            m_rightPane->setCurrentWidget(m_dataIdentifierTable);
            return;
        }
        break;
    case NavigationModel::SessionItem:
        m_shownSessionId = current.data(NavigationModel::ObjectIdRole).toInt();
        refreshSessionProperties();
        return;
    case NavigationModel::ResetTypeItem:
        m_shownResetTypeId = current.data(NavigationModel::ObjectIdRole).toInt();
        refreshResetTypeProperties();
        return;
    case NavigationModel::SecurityLevelItem:
        m_shownSecurityLevelId = current.data(NavigationModel::ObjectIdRole).toInt();
        refreshSecurityLevelProperties();
        return;
    case NavigationModel::ValueTableItem:
        m_shownValueTableId = current.data(NavigationModel::ObjectIdRole).toInt();
        refreshValueTableProperties();
        return;
    case NavigationModel::DataTypeItem:
        m_shownDataTypeId = current.data(NavigationModel::ObjectIdRole).toInt();
        refreshDataTypeProperties();
        return;
    case NavigationModel::DataIdentifierItem:
    case NavigationModel::ReadDataIdentifierItem:
    case NavigationModel::WriteDataIdentifierItem:
        m_shownDataIdentifierId = current.data(NavigationModel::ObjectIdRole).toInt();
        refreshDataIdentifierProperties();
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

void MainWindow::refreshResetTypeProperties()
{
    if (m_shownResetTypeId == 0)
        return; // no reset type is shown

    if (const ResetType *resetType = m_database->findResetType(m_shownResetTypeId)) {
        m_resetTypeProperties->showResetType(*resetType, *m_database);
        m_rightPane->setCurrentWidget(m_resetTypeProperties);
    } else {
        m_shownResetTypeId = 0;
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

void MainWindow::refreshValueTableProperties()
{
    if (m_shownValueTableId == 0)
        return; // no value table is shown

    if (const ValueTable *valueTable = m_database->findValueTable(m_shownValueTableId)) {
        m_valueTableProperties->showValueTable(*valueTable, *m_database);
        m_rightPane->setCurrentWidget(m_valueTableProperties);
    } else {
        m_shownValueTableId = 0;
        m_rightPane->setCurrentWidget(m_emptyPage);
    }
}

void MainWindow::refreshDataTypeProperties()
{
    if (m_shownDataTypeId == 0)
        return; // no data type is shown

    if (const DataType *dataType = m_database->findDataType(m_shownDataTypeId)) {
        m_dataTypeProperties->showDataType(*dataType, *m_database);
        m_rightPane->setCurrentWidget(m_dataTypeProperties);
    } else {
        m_shownDataTypeId = 0;
        m_rightPane->setCurrentWidget(m_emptyPage);
    }
}

void MainWindow::refreshDataIdentifierProperties()
{
    if (m_shownDataIdentifierId == 0)
        return; // no DID is shown

    if (const DataIdentifier *dataIdentifier = m_database->findDataIdentifier(m_shownDataIdentifierId)) {
        m_dataIdentifierProperties->showDataIdentifier(*dataIdentifier, *m_database);
        m_rightPane->setCurrentWidget(m_dataIdentifierProperties);
    } else {
        m_shownDataIdentifierId = 0;
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

    // The default session and a session that a security level or reset type is
    // allowed in cannot be deleted (Database refuses it too).
    if (session->sessionType == DefaultSessionType) {
        QMessageBox::information(this, "Delete Session",
                                 QString("The session '%1' cannot be deleted: every ECU has the "
                                         "default session.").arg(label));
        return;
    }
    QStringList usedBy;
    for (const QString &name : m_database->securityLevelsUsingSession(id))
        usedBy << name + " (security level)";
    for (const QString &name : m_database->resetTypesUsingSession(id))
        usedBy << name + " (reset type)";
    for (const QString &name : m_database->dataIdentifiersUsingSession(id))
        usedBy << name + " (DID)";
    if (!usedBy.isEmpty()) {
        QMessageBox::information(this, "Delete Session",
                                 QString("The session '%1' cannot be deleted because these are "
                                         "allowed in it:\n%2\n\nRemove it from them first.")
                                     .arg(label, usedBy.join(", ")));
        return;
    }

    const QString question = QString("Delete the session '%1'?").arg(label);
    if (QMessageBox::question(this, "Delete Session", question,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        m_database->removeSession(id);
}

void MainWindow::createNewResetType()
{
    const QStringList existingNames = m_database->resetTypeNames();
    const QVector<quint8> usedResetTypes = resetTypesUsedByOthers(*m_database, 0);

    // Pick the first free default name: New_ResetType_1, New_ResetType_2, ...
    ResetType newResetType;
    int number = 1;
    do {
        newResetType.name = QString("New_ResetType_%1").arg(number++);
    } while (existingNames.contains(newResetType.name));

    // And the first free resetType sub-function. If none is free, the dialog says so.
    for (int value = 0; value <= 0xFF; ++value) {
        const quint8 type = quint8(value);
        if (isValidResetType(type) && !usedResetTypes.contains(type)) {
            newResetType.resetType = type;
            break;
        }
    }

    ResetTypeDialog dialog(newResetType, existingNames, usedResetTypes, m_database->allSessions(),
                           m_database->allSecurityLevels(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->addResetType(dialog.resetType());
}

void MainWindow::editResetType(int id)
{
    const ResetType *stored = m_database->findResetType(id);
    if (!stored)
        return;
    const ResetType resetType = *stored; // copy: the pointer is only valid until the next change

    QStringList otherNames = m_database->resetTypeNames();
    otherNames.removeOne(resetType.name);

    ResetTypeDialog dialog(resetType, otherNames, resetTypesUsedByOthers(*m_database, id), m_database->allSessions(),
                           m_database->allSecurityLevels(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->updateResetType(dialog.resetType());
}

void MainWindow::deleteResetType(int id)
{
    const ResetType *resetType = m_database->findResetType(id);
    if (!resetType)
        return;

    const QString question = QString("Delete the reset type '%1 %2'?")
                                 .arg(subFunctionToText(resetType->resetType), resetType->name);
    if (QMessageBox::question(this, "Delete Reset Type", question,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        m_database->removeResetType(id);
}

void MainWindow::createNewDataIdentifier()
{
    const QStringList existingNames = m_database->dataIdentifierNames();
    const QVector<quint16> usedDids = didsUsedByOthers(*m_database, 0);

    // Pick the first free default name: New_DID_1, New_DID_2, ...
    DataIdentifier newDataIdentifier;
    int number = 1;
    do {
        newDataIdentifier.name = QString("New_DID_%1").arg(number++);
    } while (existingNames.contains(newDataIdentifier.name));

    // And the first free DID from 0x0100, the start of the vehicle manufacturer range.
    for (int did = 0x0100; did <= 0xFFFF; ++did) {
        if (!usedDids.contains(quint16(did))) {
            newDataIdentifier.did = quint16(did);
            break;
        }
    }

    // Not readable or writable yet: that is set under 0x22 and 0x2E.
    DataIdentifierDialog dialog(newDataIdentifier, existingNames, usedDids, m_database->allDataTypes(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->addDataIdentifier(dialog.dataIdentifier());
}

void MainWindow::editDataIdentifier(int id)
{
    const DataIdentifier *stored = m_database->findDataIdentifier(id);
    if (!stored)
        return;
    const DataIdentifier dataIdentifier = *stored; // copy: the pointer is only valid until the next change

    QStringList otherNames = m_database->dataIdentifierNames();
    otherNames.removeOne(dataIdentifier.name);

    DataIdentifierDialog dialog(dataIdentifier, otherNames, didsUsedByOthers(*m_database, id),
                                m_database->allDataTypes(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->updateDataIdentifier(dialog.dataIdentifier());
}

void MainWindow::addAccessDataIdentifier(DidAccess access)
{
    // Offer the DIDs that do not have this access yet.
    QStringList labels;
    QHash<QString, int> idByLabel;
    for (const DataIdentifier &dataIdentifier : m_database->allDataIdentifiers()) {
        if (hasAccess(dataIdentifier, access))
            continue;
        const QString label = didToText(dataIdentifier.did) + ' ' + dataIdentifier.name;
        labels << label;
        idByLabel.insert(label, dataIdentifier.id);
    }
    const QString title = QString("Add DID to %1").arg(serviceOf(access));
    if (labels.isEmpty()) {
        QMessageBox::information(this, title, QString("Every DID can be %1 already. Create new DIDs under "
                                                      "'Data Identifiers' first.").arg(verbOf(access)));
        return;
    }

    bool ok = false;
    const QString label = QInputDialog::getItem(this, title, QString("DID to make %1:").arg(adjectiveOf(access)), labels,
                                                0, false, &ok);
    if (ok)
        editAccess(idByLabel.value(label), access);
}

void MainWindow::editAccess(int id, DidAccess access)
{
    const DataIdentifier *stored = m_database->findDataIdentifier(id);
    if (!stored)
        return;
    DataIdentifier dataIdentifier = *stored; // copy: the pointer is only valid until the next change
    QVector<int> &sessionIds = access == DidAccess::Read ? dataIdentifier.readSessionIds : dataIdentifier.writeSessionIds;
    QVector<int> &securityLevelIds = access == DidAccess::Read ? dataIdentifier.readSecurityLevelIds
                                                               : dataIdentifier.writeSecurityLevelIds;

    const QString title = QString("%1 Access of DID '%2 %3'")
                              .arg(access == DidAccess::Read ? "Read" : "Write", didToText(dataIdentifier.did),
                                   dataIdentifier.name);
    AccessDialog dialog(title, verbOf(access), m_database->allSessions(), m_database->allSecurityLevels(), sessionIds,
                        securityLevelIds, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    sessionIds = dialog.sessionIds();
    securityLevelIds = dialog.securityLevelIds();
    m_database->updateDataIdentifier(dataIdentifier);
}

void MainWindow::removeAccess(int id, DidAccess access)
{
    const DataIdentifier *stored = m_database->findDataIdentifier(id);
    if (!stored)
        return;
    DataIdentifier dataIdentifier = *stored; // copy: the pointer is only valid until the next change

    const QString question = QString("Make the DID '%1 %2' not %3 by %4?\n\n"
                                     "The DID itself stays under 'Data Identifiers'.")
                                 .arg(didToText(dataIdentifier.did), dataIdentifier.name, adjectiveOf(access),
                                      serviceOf(access));
    if (QMessageBox::question(this, QString("Remove DID from %1").arg(serviceOf(access)), question,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    if (access == DidAccess::Read) {
        dataIdentifier.readSessionIds.clear();
        dataIdentifier.readSecurityLevelIds.clear();
    } else {
        dataIdentifier.writeSessionIds.clear();
        dataIdentifier.writeSecurityLevelIds.clear();
    }
    m_database->updateDataIdentifier(dataIdentifier);
}

void MainWindow::deleteDataIdentifier(int id)
{
    const DataIdentifier *dataIdentifier = m_database->findDataIdentifier(id);
    if (!dataIdentifier)
        return;

    const QString question = QString("Delete the DID '%1 %2'?").arg(didToText(dataIdentifier->did), dataIdentifier->name);
    if (QMessageBox::question(this, "Delete DID", question,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        m_database->removeDataIdentifier(id);
}

void MainWindow::createNewDataType()
{
    const QStringList existingNames = m_database->dataTypeNames();

    // Pick the first free default name: New_DataType_1, New_DataType_2, ...
    DataType newDataType;
    int number = 1;
    do {
        newDataType.name = QString("New_DataType_%1").arg(number++);
    } while (existingNames.contains(newDataType.name));

    DataTypeDialog dialog(newDataType, existingNames, m_database->allValueTables(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_database->addDataType(dialog.dataType());
}

void MainWindow::editDataType(int id)
{
    const DataType *stored = m_database->findDataType(id);
    if (!stored)
        return;
    const DataType dataType = *stored; // copy: the pointer is only valid until the next change

    QStringList otherNames = m_database->dataTypeNames();
    otherNames.removeOne(dataType.name);

    // A change that makes a DID's data types overlap or not fit is refused (Database
    // refuses it too); the dialog opens again with the edits, so they are not lost.
    DataType edited = dataType;
    for (;;) {
        DataTypeDialog dialog(edited, otherNames, m_database->allValueTables(), this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        edited = dialog.dataType();
        const QString error = m_database->checkDataTypeChange(edited);
        if (error.isEmpty())
            break;
        QMessageBox::warning(this, "Data Type", QString("This change does not fit a DID that places the data type:\n%1")
                                                    .arg(error));
    }

    m_database->updateDataType(edited);
}

void MainWindow::deleteDataType(int id)
{
    const DataType *dataType = m_database->findDataType(id);
    if (!dataType)
        return;

    // A data type that a DID places cannot be deleted (Database refuses it too).
    const QStringList usedBy = m_database->dataIdentifiersUsingDataType(id);
    if (!usedBy.isEmpty()) {
        QMessageBox::information(this, "Delete Data Type",
                                 QString("The data type '%1' cannot be deleted because these DIDs "
                                         "place it:\n%2\n\nRemove it from them first.")
                                     .arg(dataType->name, usedBy.join(", ")));
        return;
    }

    const QString question = QString("Delete the data type '%1'?").arg(dataType->name);
    if (QMessageBox::question(this, "Delete Data Type", question,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        m_database->removeDataType(id);
}

void MainWindow::createNewValueTable()
{
    const QStringList existingNames = m_database->valueTableNames();

    // Pick the first free default name: New_ValueTable_1, New_ValueTable_2, ...
    ValueTable newValueTable;
    int number = 1;
    do {
        newValueTable.name = QString("New_ValueTable_%1").arg(number++);
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

    // A value table that a data type uses cannot be deleted (Database refuses it too).
    const QStringList usedBy = m_database->dataTypesUsingValueTable(id);
    if (!usedBy.isEmpty()) {
        QMessageBox::information(this, "Delete Value Table",
                                 QString("The value table '%1' cannot be deleted because these "
                                         "data types use it:\n%2\n\nChoose another value table for them first.")
                                     .arg(valueTable->name, usedBy.join(", ")));
        return;
    }

    const QString question = QString("Delete the value table '%1'?").arg(valueTable->name);
    if (QMessageBox::question(this, "Delete Value Table", question,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        m_database->removeValueTable(id);
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

    const QString label = subFunctionToText(securityLevel->requestSeed) + ' ' + securityLevel->name;

    // A security level that a reset type needs cannot be deleted (Database refuses it too).
    QStringList usedBy;
    for (const QString &name : m_database->resetTypesUsingSecurityLevel(id))
        usedBy << name + " (reset type)";
    for (const QString &name : m_database->dataIdentifiersUsingSecurityLevel(id))
        usedBy << name + " (DID)";
    if (!usedBy.isEmpty()) {
        QMessageBox::information(this, "Delete Security Level",
                                 QString("The security level '%1' cannot be deleted because these "
                                         "need it:\n%2\n\nRemove it from them first.")
                                     .arg(label, usedBy.join(", ")));
        return;
    }

    const QString question = QString("Delete the security level '%1'?").arg(label);
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

    m_database->replaceAll(Network(), {}, {}, {}, {}, {}, {});
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
