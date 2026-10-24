#ifndef SECURITYLEVELTABLEMODEL_H
#define SECURITYLEVELTABLEMODEL_H

#include <QAbstractTableModel>

namespace usc {
class Database;
} // namespace usc

using namespace usc;

// Table of all security levels in the Database: one row per security level,
// one column per property. It holds no data of its own and reads everything from Database.
class SecurityLevelTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { Name, RequestSeed, SendKey, SeedLength, KeyLength, MaxAttempts, AllowedSessions, Comment, ColumnCount };

    // Value to sort by: numbers as numbers (so 0x09 comes before 0x11).
    static constexpr int SortRole = Qt::UserRole + 1;

    explicit SecurityLevelTableModel(Database *database, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    Database *m_database = nullptr;
};

#endif // SECURITYLEVELTABLEMODEL_H
