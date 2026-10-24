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

    connect(m_database, &Database::aboutToBeReset, this, &NavigationModel::beginResetModel);
    connect(m_database, &Database::wasReset, this, &NavigationModel::endResetModel);
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
    case SecurityAccess: return m_database->allSecurityLevels().size();
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
        static const char *const headings[CategoryCount] = {"Network", "0x27 SecurityAccess"};
        static const char *const icons[CategoryCount] = {":/icons/network.svg", ":/icons/securityaccess.svg"};
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
    case SecurityAccess: {
        const SecurityLevel &securityLevel = m_database->allSecurityLevels().at(index.row());
        name = subFunctionToText(securityLevel.requestSeed) + ' ' + securityLevel.name; icon = ":/icons/securityaccess.svg"; kind = SecurityLevelItem;
        id = securityLevel.id;
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
