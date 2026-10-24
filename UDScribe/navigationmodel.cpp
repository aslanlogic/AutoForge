#include "navigationmodel.h"
#include "usccore/database.h"

#include <QIcon>

using namespace usc;

// How rows are identified. Every index carries a number (internalId) that is
// enough to find its parent:
//
//   heading                0
//   item under a heading   Category + 1   (1 .. CategoryCount)
namespace {

bool isHeading(const QModelIndex &index) { return index.internalId() == 0; }

} // namespace

NavigationModel::NavigationModel(Database *database, QObject *parent)
    : QAbstractItemModel(parent)
    , m_database(database)
{
    connect(m_database, &Database::sessionAboutToBeAdded, this, [this](int index) {
        beginInsertRows(categoryIndex(DiagnosticSessionControl), index, index);
    });
    connect(m_database, &Database::sessionAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::sessionChanged, this, [this](int id) {
        const QModelIndex changed = index(m_database->indexOfSession(id), 0, categoryIndex(DiagnosticSessionControl));
        emit dataChanged(changed, changed);
    });
    connect(m_database, &Database::sessionAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(categoryIndex(DiagnosticSessionControl), index, index);
    });
    connect(m_database, &Database::sessionRemoved, this, [this] {
        endRemoveRows();
    });

    connect(m_database, &Database::resetTypeAboutToBeAdded, this, [this](int index) {
        beginInsertRows(categoryIndex(EcuReset), index, index);
    });
    connect(m_database, &Database::resetTypeAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::resetTypeChanged, this, [this](int id) {
        const QModelIndex changed = index(m_database->indexOfResetType(id), 0, categoryIndex(EcuReset));
        emit dataChanged(changed, changed);
    });
    connect(m_database, &Database::resetTypeAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(categoryIndex(EcuReset), index, index);
    });
    connect(m_database, &Database::resetTypeRemoved, this, [this] {
        endRemoveRows();
    });

    connect(m_database, &Database::securityLevelAboutToBeAdded, this, [this](int index) {
        beginInsertRows(categoryIndex(SecurityAccess), index, index);
    });
    connect(m_database, &Database::securityLevelAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::securityLevelChanged, this, [this](int id) {
        const QModelIndex changed = index(m_database->indexOfSecurityLevel(id), 0, categoryIndex(SecurityAccess));
        emit dataChanged(changed, changed);
    });
    connect(m_database, &Database::securityLevelAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(categoryIndex(SecurityAccess), index, index);
    });
    connect(m_database, &Database::securityLevelRemoved, this, [this] {
        endRemoveRows();
    });

    connect(m_database, &Database::valueTableAboutToBeAdded, this, [this](int index) {
        beginInsertRows(categoryIndex(ValueTables), index, index);
    });
    connect(m_database, &Database::valueTableAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::valueTableChanged, this, [this](int id) {
        const QModelIndex changed = index(m_database->indexOfValueTable(id), 0, categoryIndex(ValueTables));
        emit dataChanged(changed, changed);
    });
    connect(m_database, &Database::valueTableAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(categoryIndex(ValueTables), index, index);
    });
    connect(m_database, &Database::valueTableRemoved, this, [this] {
        endRemoveRows();
    });

    connect(m_database, &Database::dataTypeAboutToBeAdded, this, [this](int index) {
        beginInsertRows(categoryIndex(DataTypes), index, index);
    });
    connect(m_database, &Database::dataTypeAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::dataTypeChanged, this, [this](int id) {
        const QModelIndex changed = index(m_database->indexOfDataType(id), 0, categoryIndex(DataTypes));
        emit dataChanged(changed, changed);
    });
    connect(m_database, &Database::dataTypeAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(categoryIndex(DataTypes), index, index);
    });
    connect(m_database, &Database::dataTypeRemoved, this, [this] {
        endRemoveRows();
    });

    // A DID also appears under 0x22 if it can be read, and under 0x2E if it can be written.
    connect(m_database, &Database::dataIdentifierAboutToBeAdded, this, [this](int index) {
        beginInsertRows(categoryIndex(DataIdentifiers), index, index);
    });
    connect(m_database, &Database::dataIdentifierAdded, this, [this] {
        endInsertRows();
        updateAccessDataIdentifiers();
    });
    connect(m_database, &Database::dataIdentifierChanged, this, [this](int id) {
        const QModelIndex changed = index(m_database->indexOfDataIdentifier(id), 0, categoryIndex(DataIdentifiers));
        emit dataChanged(changed, changed);
        updateAccessDataIdentifiers();
    });
    connect(m_database, &Database::dataIdentifierAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(categoryIndex(DataIdentifiers), index, index);
    });
    connect(m_database, &Database::dataIdentifierRemoved, this, [this] {
        endRemoveRows();
        updateAccessDataIdentifiers();
    });

    connect(m_database, &Database::aboutToBeReset, this, &NavigationModel::beginResetModel);
    connect(m_database, &Database::wasReset, this, [this] {
        m_readDataIdentifierIds = dataIdentifiersWith(DidAccess::Read);
        m_writeDataIdentifierIds = dataIdentifiersWith(DidAccess::Write);
        endResetModel();
    });
}

QVector<int> NavigationModel::dataIdentifiersWith(DidAccess access) const
{
    QVector<int> ids;
    for (const DataIdentifier &dataIdentifier : m_database->allDataIdentifiers()) {
        if (hasAccess(dataIdentifier, access))
            ids.append(dataIdentifier.id);
    }
    return ids;
}

QVector<int> &NavigationModel::cachedDataIdentifiers(DidAccess access)
{
    return access == DidAccess::Read ? m_readDataIdentifierIds : m_writeDataIdentifierIds;
}

const QVector<int> &NavigationModel::cachedDataIdentifiers(DidAccess access) const
{
    return access == DidAccess::Read ? m_readDataIdentifierIds : m_writeDataIdentifierIds;
}

void NavigationModel::updateAccessDataIdentifiers()
{
    updateDataIdentifiers(DidAccess::Read);
    updateDataIdentifiers(DidAccess::Write);
}

void NavigationModel::updateDataIdentifiers(DidAccess access)
{
    const QModelIndex heading = categoryIndex(access == DidAccess::Read ? ReadDataByIdentifier : WriteDataByIdentifier);
    QVector<int> &cached = cachedDataIdentifiers(access);
    const QVector<int> current = dataIdentifiersWith(access);
    if (current == cached) {
        // Same DIDs; a name or number may still have changed.
        if (!current.isEmpty())
            emit dataChanged(index(0, 0, heading), index(current.size() - 1, 0, heading));
        return;
    }

    // Simplest correct way to tell the views: remove all old rows, insert all new ones.
    if (!cached.isEmpty()) {
        beginRemoveRows(heading, 0, cached.size() - 1);
        cached.clear();
        endRemoveRows();
    }
    if (!current.isEmpty()) {
        beginInsertRows(heading, 0, current.size() - 1);
        cached = current;
        endInsertRows();
    }
}

QModelIndex NavigationModel::categoryIndex(Category category) const
{
    return createIndex(category, 0, quintptr(0));
}

QModelIndex NavigationModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    if (!parent.isValid())
        return createIndex(row, column, quintptr(0));
    return createIndex(row, column, quintptr(parent.row() + 1));
}

QModelIndex NavigationModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || isHeading(child))
        return QModelIndex();
    return categoryIndex(static_cast<Category>(child.internalId() - 1));
}

int NavigationModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return CategoryCount;
    if (parent.column() > 0 || !isHeading(parent))
        return 0;

    switch (parent.row()) {
    case DiagnosticSessionControl: return m_database->allSessions().size();
    case EcuReset:                 return m_database->allResetTypes().size();
    case ReadDataByIdentifier:     return m_readDataIdentifierIds.size();
    case WriteDataByIdentifier:    return m_writeDataIdentifierIds.size();
    case SecurityAccess:           return m_database->allSecurityLevels().size();
    case ValueTables:              return m_database->allValueTables().size();
    case DataTypes:                return m_database->allDataTypes().size();
    case DataIdentifiers:          return m_database->allDataIdentifiers().size();
    }
    return 0;
}

int NavigationModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant NavigationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    // Icons are compiled into the program from resources.qrc.
    if (isHeading(index)) {
        static const char *const headings[CategoryCount] = {
            "Network", "0x10 DiagnosticSessionControl", "0x11 ECUReset", "0x22 ReadDataByIdentifier",
            "0x27 SecurityAccess", "0x2E WriteDataByIdentifier", "Value Tables",
            "Data Types", "Data Identifiers"};
        static const char *const icons[CategoryCount] = {
            ":/icons/network.svg", ":/icons/session.svg", ":/icons/ecureset.svg", ":/icons/dataidentifiers.svg",
            ":/icons/securityaccess.svg", ":/icons/dataidentifiers.svg",
            ":/icons/valuetables.svg", ":/icons/datatypes.svg", ":/icons/dataidentifiers.svg"};
        switch (role) {
        case Qt::DisplayRole:    return QString(headings[index.row()]);
        case Qt::DecorationRole: return QIcon(icons[index.row()]);
        case ItemKindRole:       return Heading;
        }
        return QVariant();
    }

    // Name, icon, kind and id of one row; filled in below.
    QString name;
    QString icon;
    ItemKind kind = Heading;
    int id = 0;

    switch (index.internalId() - 1) {
    case DiagnosticSessionControl: {
        const Session &session = m_database->allSessions().at(index.row());
        name = subFunctionToText(session.sessionType) + ' ' + session.name; icon = ":/icons/session.svg"; kind = SessionItem;
        id = session.id;
        break;
    }
    case EcuReset: {
        const ResetType &resetType = m_database->allResetTypes().at(index.row());
        name = subFunctionToText(resetType.resetType) + ' ' + resetType.name; icon = ":/icons/ecureset.svg"; kind = ResetTypeItem;
        id = resetType.id;
        break;
    }
    case ReadDataByIdentifier:
    case WriteDataByIdentifier: {
        const bool read = int(index.internalId() - 1) == ReadDataByIdentifier;
        id = cachedDataIdentifiers(read ? DidAccess::Read : DidAccess::Write).value(index.row());
        const DataIdentifier *dataIdentifier = m_database->findDataIdentifier(id);
        name = dataIdentifier ? didToText(dataIdentifier->did) + ' ' + dataIdentifier->name : QString();
        icon = ":/icons/dataidentifiers.svg"; kind = read ? ReadDataIdentifierItem : WriteDataIdentifierItem;
        break;
    }
    case SecurityAccess: {
        const SecurityLevel &securityLevel = m_database->allSecurityLevels().at(index.row());
        name = subFunctionToText(securityLevel.requestSeed) + ' ' + securityLevel.name; icon = ":/icons/securityaccess.svg"; kind = SecurityLevelItem;
        id = securityLevel.id;
        break;
    }
    case ValueTables: {
        const ValueTable &valueTable = m_database->allValueTables().at(index.row());
        name = valueTable.name; icon = ":/icons/valuetables.svg"; kind = ValueTableItem; id = valueTable.id;
        break;
    }
    case DataTypes: {
        const DataType &dataType = m_database->allDataTypes().at(index.row());
        name = dataType.name; icon = ":/icons/datatypes.svg"; kind = DataTypeItem; id = dataType.id;
        break;
    }
    case DataIdentifiers: {
        const DataIdentifier &dataIdentifier = m_database->allDataIdentifiers().at(index.row());
        name = didToText(dataIdentifier.did) + ' ' + dataIdentifier.name; icon = ":/icons/dataidentifiers.svg";
        kind = DataIdentifierItem; id = dataIdentifier.id;
        break;
    }
    }

    switch (role) {
    case Qt::DisplayRole:    return name;
    case Qt::DecorationRole: return QIcon(icon);
    case ObjectIdRole:       return id;
    case ItemKindRole:       return kind;
    }
    return QVariant();
}
