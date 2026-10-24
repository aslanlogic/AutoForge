#ifndef DATATYPETABLEMODEL_H
#define DATATYPETABLEMODEL_H

#include <QAbstractTableModel>

namespace usc {
class Database;
} // namespace usc

using namespace usc;

// Table of all data types in the Database: one row per data type, one column per property.
// Like NavigationModel, it holds no data of its own and reads everything from Database.
class DataTypeTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        Name, Length, ByteOrder, ValueType, Unit,
        Factor, Offset, Minimum, Maximum, ValueTableName, DataIdentifierCount, Comment, ColumnCount
    };

    // Value to sort by: numbers as numbers, so 10 sorts after 9.
    static constexpr int SortRole = Qt::UserRole + 1;

    explicit DataTypeTableModel(Database *database, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    Database *m_database = nullptr;
};

#endif // DATATYPETABLEMODEL_H
