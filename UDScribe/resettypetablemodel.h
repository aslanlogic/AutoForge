#ifndef RESETTYPETABLEMODEL_H
#define RESETTYPETABLEMODEL_H

#include <QAbstractTableModel>

namespace usc {
class Database;
} // namespace usc

using namespace usc;

// Table of all reset types in the Database: one row per reset type, one column per property. It holds no data of its own and reads everything from Database.
class ResetTypeTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { Name, Type, AllowedSessions, SecurityLevels, PowerDownTime, Comment, ColumnCount };

    // Value to sort by: numbers as numbers (so 0x03 comes before 0x40).
    static constexpr int SortRole = Qt::UserRole + 1;

    explicit ResetTypeTableModel(Database *database, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    void namesChanged(int column); // a session or security level was renamed

    Database *m_database = nullptr;
};

#endif // RESETTYPETABLEMODEL_H
