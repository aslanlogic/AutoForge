#ifndef SIGNALTABLEMODEL_H
#define SIGNALTABLEMODEL_H

#include <QAbstractTableModel>

class Database;

// Table of all signals in the Database: one row per signal, one column per property.
// Like NavigationModel, it holds no data of its own and reads everything from Database.
class SignalTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        Name, Length, ByteOrder, ValueType, Unit, InitialValue,
        Factor, Offset, Minimum, Maximum, Comment, ColumnCount
    };

    // Value to sort by: numbers as numbers, so 10 sorts after 9.
    static constexpr int SortRole = Qt::UserRole + 1;

    explicit SignalTableModel(Database *database, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    Database *m_database = nullptr;
};

#endif // SIGNALTABLEMODEL_H
