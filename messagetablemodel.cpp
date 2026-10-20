#include "messagetablemodel.h"
#include "database.h"

MessageTableModel::MessageTableModel(Database *database, QObject *parent)
    : QAbstractTableModel(parent)
    , m_database(database)
{
    connect(m_database, &Database::messageAboutToBeAdded, this, [this](int index) {
        beginInsertRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::messageAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::messageChanged, this, [this](int id) {
        const int row = m_database->indexOfMessage(id);
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    });
    connect(m_database, &Database::messageAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::messageRemoved, this, [this] {
        endRemoveRows();
    });
    connect(m_database, &Database::aboutToBeReset, this, &MessageTableModel::beginResetModel);
    connect(m_database, &Database::wasReset, this, &MessageTableModel::endResetModel);
}

int MessageTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_database->allMessages().size();
}

int MessageTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant MessageTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole && role != SortRole))
        return QVariant();

    const Message &message = m_database->allMessages().at(index.row());
    const bool sorting = (role == SortRole);

    switch (index.column()) {
    case Name:        return message.name;
    case Type:        return toString(message.type);
    case FrameId:     return sorting ? QVariant(message.frameId) : QVariant(frameIdToText(message.frameId));
    case Length:      return message.length;
    case TxMethod:    return toString(message.txMethod);
    case CycleTime:
        // Not used for "On Event": show nothing, sort before every real cycle time.
        if (!isCyclic(message.txMethod))
            return sorting ? QVariant(0) : QVariant(QString());
        return message.cycleTime;
    case SignalCount: return message.signalMappings.size();
    case Comment:     return message.comment;
    }
    return QVariant();
}

QVariant MessageTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    static const char *const headers[ColumnCount] = {
        "Name", "Type", "ID", "Length [Byte]", "Tx Method", "Cycle Time [ms]", "Signals", "Comment"};
    return QString(headers[section]);
}
