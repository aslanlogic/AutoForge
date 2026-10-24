#include "datatypetablemodel.h"
#include "usccore/database.h"

#include <QLocale>

using namespace usc;

DataTypeTableModel::DataTypeTableModel(Database *database, QObject *parent)
    : QAbstractTableModel(parent)
    , m_database(database)
{
    connect(m_database, &Database::dataTypeAboutToBeAdded, this, [this](int index) {
        beginInsertRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::dataTypeAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::dataTypeChanged, this, [this](int id) {
        const int row = m_database->indexOfDataType(id);
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    });
    connect(m_database, &Database::dataTypeAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::dataTypeRemoved, this, [this] {
        endRemoveRows();
    });
    // The Value Table column shows the table's name.
    connect(m_database, &Database::valueTableChanged, this, [this] {
        if (rowCount() > 0)
            emit dataChanged(index(0, ValueTableName), index(rowCount() - 1, ValueTableName));
    });
    // The DIDs count depends on which DIDs place which data type.
    for (auto didChange : {&Database::dataIdentifierAdded, &Database::dataIdentifierChanged, &Database::dataIdentifierRemoved}) {
        connect(m_database, didChange, this, [this] {
            if (rowCount() > 0)
                emit dataChanged(index(0, DataIdentifierCount), index(rowCount() - 1, DataIdentifierCount));
        });
    }
    connect(m_database, &Database::aboutToBeReset, this, &DataTypeTableModel::beginResetModel);
    connect(m_database, &Database::wasReset, this, &DataTypeTableModel::endResetModel);
}

int DataTypeTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_database->allDataTypes().size();
}

int DataTypeTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant DataTypeTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole && role != SortRole))
        return QVariant();

    const DataType &dataType = m_database->allDataTypes().at(index.row());
    const bool sorting = (role == SortRole);
    auto number = [sorting](double value) -> QVariant {
        if (sorting)
            return value;
        return QLocale::c().toString(value, 'g', QLocale::FloatingPointShortest);
    };

    switch (index.column()) {
    case Name:         return dataType.name;
    case Length:       return dataType.length;
    case ByteOrder:    return toString(dataType.byteOrder);
    case ValueType:    return toString(dataType.valueType);
    case Unit:         return dataType.unit;
    case Factor:       return number(dataType.factor);
    case Offset:       return number(dataType.offset);
    case Minimum:      return number(dataType.minimum);
    case Maximum:      return number(dataType.maximum);
    case ValueTableName: {
        const ValueTable *valueTable = m_database->findValueTable(dataType.valueTableId);
        return valueTable ? valueTable->name : QString();
    }
    case DataIdentifierCount: return m_database->dataIdentifiersUsingDataType(dataType.id).size();
    case Comment:      return dataType.comment;
    }
    return QVariant();
}

QVariant DataTypeTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    static const char *const headers[ColumnCount] = {
        "Name", "Length [Bit]", "Byte Order", "Value Type", "Unit",
        "Factor", "Offset", "Minimum", "Maximum", "Value Table", "DIDs", "Comment"};
    return QString(headers[section]);
}
