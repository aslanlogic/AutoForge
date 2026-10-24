#include "sessiontablemodel.h"
#include "usccore/database.h"

using namespace usc;

SessionTableModel::SessionTableModel(Database *database, QObject *parent)
    : QAbstractTableModel(parent)
    , m_database(database)
{
    connect(m_database, &Database::sessionAboutToBeAdded, this, [this](int index) {
        beginInsertRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::sessionAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::sessionChanged, this, [this](int id) {
        const int row = m_database->indexOfSession(id);
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    });
    connect(m_database, &Database::sessionAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::sessionRemoved, this, [this] {
        endRemoveRows();
    });

    connect(m_database, &Database::aboutToBeReset, this, &SessionTableModel::beginResetModel);
    connect(m_database, &Database::wasReset, this, &SessionTableModel::endResetModel);
}

int SessionTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_database->allSessions().size();
}

int SessionTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant SessionTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole && role != SortRole))
        return QVariant();

    const Session &session = m_database->allSessions().at(index.row());
    switch (index.column()) {
    case Name:
        return session.name;
    case SessionType:
        if (role == SortRole)
            return int(session.sessionType);
        return subFunctionToText(session.sessionType);
    case P2:
        if (role == SortRole)
            return session.p2;
        return QString("%1 ms").arg(session.p2);
    case P2Star:
        if (role == SortRole)
            return session.p2Star;
        return QString("%1 ms").arg(session.p2Star);
    case Comment:
        return session.comment;
    }
    return QVariant();
}

QVariant SessionTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    static const char *const headers[ColumnCount] = {"Name", "Session Type", "P2", "P2*", "Comment"};
    return QString(headers[section]);
}
