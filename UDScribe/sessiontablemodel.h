#ifndef SESSIONTABLEMODEL_H
#define SESSIONTABLEMODEL_H

#include <QAbstractTableModel>

namespace usc {
class Database;
} // namespace usc

using namespace usc;

// Table of all sessions in the Database: one row per session, one column per property. It holds no data of its own and reads everything from Database.
class SessionTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { Name, SessionType, P2, P2Star, Comment, ColumnCount };

    // Value to sort by: numbers as numbers (so 0x03 comes before 0x40).
    static constexpr int SortRole = Qt::UserRole + 1;

    explicit SessionTableModel(Database *database, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    Database *m_database = nullptr;
};

#endif // SESSIONTABLEMODEL_H
