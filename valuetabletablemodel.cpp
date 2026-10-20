#include "valuetabletablemodel.h"
#include "database.h"

ValueTableTableModel::ValueTableTableModel(Database *database, QObject *parent)
    : QAbstractTableModel(parent)
    , m_database(database)
{
    connect(m_database, &Database::valueTableAboutToBeAdded, this, [this](int index) {
        beginInsertRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::valueTableAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::valueTableChanged, this, [this](int id) {
        const int row = m_database->indexOfValueTable(id);
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    });
    connect(m_database, &Database::valueTableAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::valueTableRemoved, this, [this] {
        endRemoveRows();
    });

    // The Signals count depends on which signals use which table.
    connect(m_database, &Database::signalAdded, this, &ValueTableTableModel::signalCountsChanged);
    connect(m_database, &Database::signalChanged, this, &ValueTableTableModel::signalCountsChanged);
    connect(m_database, &Database::signalRemoved, this, &ValueTableTableModel::signalCountsChanged);

    connect(m_database, &Database::aboutToBeReset, this, &ValueTableTableModel::beginResetModel);
    connect(m_database, &Database::wasReset, this, &ValueTableTableModel::endResetModel);
}

void ValueTableTableModel::signalCountsChanged()
{
    if (rowCount() > 0)
        emit dataChanged(index(0, SignalCount), index(rowCount() - 1, SignalCount));
}

int ValueTableTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_database->allValueTables().size();
}

int ValueTableTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ValueTableTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole && role != SortRole))
        return QVariant();

    const ValueTable &valueTable = m_database->allValueTables().at(index.row());
    switch (index.column()) {
    case Name:        return valueTable.name;
    case EntryCount:  return valueTable.entries.size();
    case SignalCount: return m_database->signalsUsingValueTable(valueTable.id).size();
    case Comment:     return valueTable.comment;
    }
    return QVariant();
}

QVariant ValueTableTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    static const char *const headers[ColumnCount] = {"Name", "Values", "Signals", "Comment"};
    return QString(headers[section]);
}
