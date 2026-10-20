#include "navigationmodel.h"
#include "database.h"

#include <QIcon>

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
    connect(m_database, &Database::signalChanged, this, [this](int id) {
        const QModelIndex changed = index(m_database->indexOfSignal(id), 0, categoryIndex(Signals));
        emit dataChanged(changed, changed);
    });
    connect(m_database, &Database::signalAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(categoryIndex(Signals), index, index);
    });
    connect(m_database, &Database::signalRemoved, this, [this] {
        endRemoveRows();
    });
    connect(m_database, &Database::messageAboutToBeAdded, this, [this](int index) {
        beginInsertRows(categoryIndex(Messages), index, index);
    });
    connect(m_database, &Database::messageAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::messageChanged, this, [this](int id) {
        const QModelIndex changed = index(m_database->indexOfMessage(id), 0, categoryIndex(Messages));
        emit dataChanged(changed, changed);
    });
    connect(m_database, &Database::messageAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(categoryIndex(Messages), index, index);
    });
    connect(m_database, &Database::messageRemoved, this, [this] {
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
    if (parent.row() == Messages)
        return m_database->allMessages().size();

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
        if (role == Qt::DisplayRole) {
            static const char *const headings[CategoryCount] = {
                "Network", "Network Nodes", "Messages", "Signals"};
            return QString(headings[index.row()]);
        }
        if (role == Qt::DecorationRole) {
            // Icons are compiled into the program from resources.qrc.
            static const char *const icons[CategoryCount] = {
                ":/icons/network.svg", ":/icons/nodes.svg",
                ":/icons/messages.svg", ":/icons/signals.svg"};
            return QIcon(icons[index.row()]);
        }
        return QVariant();
    }

    if (index.internalId() - 1 == Signals) {
        const Signal &signal = m_database->allSignals().at(index.row());
        if (role == Qt::DisplayRole)
            return signal.name;
        if (role == Qt::DecorationRole)
            return QIcon(":/icons/signals.svg");
        if (role == ObjectIdRole)
            return signal.id;
    }

    if (index.internalId() - 1 == Messages) {
        const Message &message = m_database->allMessages().at(index.row());
        if (role == Qt::DisplayRole)
            return message.name;
        if (role == Qt::DecorationRole)
            return QIcon(":/icons/messages.svg");
        if (role == ObjectIdRole)
            return message.id;
    }

    return QVariant();
}
