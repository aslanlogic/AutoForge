#ifndef MESSAGETABLEMODEL_H
#define MESSAGETABLEMODEL_H

#include <QAbstractTableModel>

namespace ff {
class Database;
} // namespace ff

using namespace ff;

// Table of all messages in the Database: one row per message, one column per property.
// Works like SignalTableModel: it holds no data of its own and reads everything from Database.
class MessageTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        Name, IdFormat, FrameId, Length, Transmitter, Receivers, TxMethod, CycleTime, SignalCount, Comment,
        ColumnCount
    };

    // Value to sort by: numbers as numbers, so 0x100 sorts after 0xFF.
    static constexpr int SortRole = Qt::UserRole + 1;

    explicit MessageTableModel(Database *database, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    Database *m_database = nullptr;
};

#endif // MESSAGETABLEMODEL_H
