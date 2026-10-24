#ifndef VALUETABLETABLEMODEL_H
#define VALUETABLETABLEMODEL_H

#include <QAbstractTableModel>

namespace usc {
class Database;
} // namespace usc

using namespace usc;

// Table of all value tables in the Database: one row per value table.
// It holds no data of its own and reads everything from Database.
class ValueTableTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { Name, EntryCount, DataTypeCount, Comment, ColumnCount };

    // Value to sort by: numbers as numbers.
    static constexpr int SortRole = Qt::UserRole + 1;

    explicit ValueTableTableModel(Database *database, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    void dataTypeCountsChanged(); // a data type was added, changed or deleted

    Database *m_database = nullptr;
};

#endif // VALUETABLETABLEMODEL_H
