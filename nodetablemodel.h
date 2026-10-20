#ifndef NODETABLEMODEL_H
#define NODETABLEMODEL_H

#include <QAbstractTableModel>

class Database;

// Table of all network nodes in the Database: one row per node.
// Works like SignalTableModel: it holds no data of its own and reads everything from Database.
class NodeTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { Name, TxCount, RxCount, Comment, ColumnCount };

    // Value to sort by: numbers as numbers.
    static constexpr int SortRole = Qt::UserRole + 1;

    explicit NodeTableModel(Database *database, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    void countsChanged(); // a message was added, changed or deleted

    Database *m_database = nullptr;
};

#endif // NODETABLEMODEL_H
