#include "signaltablemodel.h"
#include "database.h"

#include <QLocale>

SignalTableModel::SignalTableModel(Database *database, QObject *parent)
    : QAbstractTableModel(parent)
    , m_database(database)
{
    connect(m_database, &Database::signalAboutToBeAdded, this, [this](int index) {
        beginInsertRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::signalAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::signalChanged, this, [this](int id) {
        const int row = m_database->indexOfSignal(id);
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    });
    connect(m_database, &Database::signalAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::signalRemoved, this, [this] {
        endRemoveRows();
    });
    connect(m_database, &Database::aboutToBeReset, this, &SignalTableModel::beginResetModel);
    connect(m_database, &Database::wasReset, this, &SignalTableModel::endResetModel);
}

int SignalTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_database->allSignals().size();
}

int SignalTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant SignalTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole && role != SortRole))
        return QVariant();

    const Signal &signal = m_database->allSignals().at(index.row());
    const bool sorting = (role == SortRole);
    auto number = [sorting](double value) -> QVariant {
        if (sorting)
            return value;
        return QLocale::c().toString(value, 'g', QLocale::FloatingPointShortest);
    };

    switch (index.column()) {
    case Name:         return signal.name;
    case Length:       return signal.length;
    case ByteOrder:    return toString(signal.byteOrder);
    case ValueType:    return toString(signal.valueType);
    case Unit:         return signal.unit;
    case InitialValue: return number(signal.initialValue);
    case Factor:       return number(signal.factor);
    case Offset:       return number(signal.offset);
    case Minimum:      return number(signal.minimum);
    case Maximum:      return number(signal.maximum);
    case Comment:      return signal.comment;
    }
    return QVariant();
}

QVariant SignalTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    static const char *const headers[ColumnCount] = {
        "Name", "Length [Bit]", "Byte Order", "Value Type", "Unit", "Init. Value",
        "Factor", "Offset", "Minimum", "Maximum", "Comment"};
    return QString(headers[section]);
}
