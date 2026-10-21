#ifndef VALUETABLETABLEMODEL_H
#define VALUETABLETABLEMODEL_H

#include <QAbstractTableModel>

namespace ff {
class Database;
} // namespace ff

using namespace ff;

// Table of all value tables in the Database: one row per value table.
// Works like SignalTableModel: it holds no data of its own and reads everything from Database.
class ValueTableTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { Name, EntryCount, SignalCount, Comment, ColumnCount };

    // Value to sort by: numbers as numbers.
    static constexpr int SortRole = Qt::UserRole + 1;

    explicit ValueTableTableModel(Database *database, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    void signalCountsChanged(); // a signal was added, changed or deleted

    Database *m_database = nullptr;
};

#endif // VALUETABLETABLEMODEL_H
