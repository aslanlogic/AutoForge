#include "navigationmodel.h"
#include "database.h"

// How rows are identified: a heading's index has internalId 0; an item's
// index has internalId = (its heading's Category + 1). That is enough to
// find the parent of any index without extra bookkeeping.

NavigationModel::NavigationModel(Database *database, QObject *parent)
    : QAbstractItemModel(parent)
    , m_database(database)
{
    connect(m_database, &Database::signalAboutToBeAdded, this, [this](int index) {
        beginInsertRows(categoryIndex(Signals), index, index);
    });
    connect(m_database, &Database::signalAdded, this, [this] {
        endInsertRows();
    });
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
    if (!child.isValid() || child.internalId() == 0)
        return QModelIndex();

    return categoryIndex(static_cast<Category>(child.internalId() - 1));
}

int NavigationModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return CategoryCount;

    // Items have no children.
    if (parent.internalId() != 0)
        return 0;

    if (parent.row() == Signals)
        return m_database->allSignals().size();

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

    if (index.internalId() == 0) {
        if (role != Qt::DisplayRole)
            return QVariant();
        static const char *const headings[CategoryCount] = {
            "Network", "Network Nodes", "Messages", "Signals"};
        return QString(headings[index.row()]);
    }

    if (index.internalId() - 1 == Signals) {
        const Signal &signal = m_database->allSignals().at(index.row());
        if (role == Qt::DisplayRole)
            return signal.name;
        if (role == ObjectIdRole)
            return signal.id;
    }

    return QVariant();
}
