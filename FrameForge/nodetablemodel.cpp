#include "nodetablemodel.h"
#include "FrameForgeCore/database.h"

using namespace ff;

NodeTableModel::NodeTableModel(Database *database, QObject *parent)
    : QAbstractTableModel(parent)
    , m_database(database)
{
    connect(m_database, &Database::nodeAboutToBeAdded, this, [this](int index) {
        beginInsertRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::nodeAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::nodeChanged, this, [this](int id) {
        const int row = m_database->indexOfNode(id);
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    });
    connect(m_database, &Database::nodeAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::nodeRemoved, this, [this] {
        endRemoveRows();
    });

    // The Tx/Rx counts depend on the messages.
    connect(m_database, &Database::messageAdded, this, &NodeTableModel::countsChanged);
    connect(m_database, &Database::messageChanged, this, &NodeTableModel::countsChanged);
    connect(m_database, &Database::messageRemoved, this, &NodeTableModel::countsChanged);

    connect(m_database, &Database::aboutToBeReset, this, &NodeTableModel::beginResetModel);
    connect(m_database, &Database::wasReset, this, &NodeTableModel::endResetModel);
}

void NodeTableModel::countsChanged()
{
    if (rowCount() > 0)
        emit dataChanged(index(0, TxCount), index(rowCount() - 1, RxCount));
}

int NodeTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_database->allNodes().size();
}

int NodeTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant NodeTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole && role != SortRole))
        return QVariant();

    const Node &node = m_database->allNodes().at(index.row());
    switch (index.column()) {
    case Name:    return node.name;
    case TxCount: return m_database->messagesTransmittedBy(node.id).size();
    case RxCount: return m_database->messagesReceivedBy(node.id).size();
    case Comment: return node.comment;
    }
    return QVariant();
}

QVariant NodeTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    static const char *const headers[ColumnCount] = {"Name", "Tx Messages", "Rx Messages", "Comment"};
    return QString(headers[section]);
}
