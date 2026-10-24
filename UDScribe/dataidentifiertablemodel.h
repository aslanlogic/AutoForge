#ifndef DATAIDENTIFIERTABLEMODEL_H
#define DATAIDENTIFIERTABLEMODEL_H

#include <QAbstractTableModel>

namespace usc {
class Database;
} // namespace usc

using namespace usc;

// Table of all DIDs in the Database: one row per DID, one column per property.
// It holds no data of its own and reads everything from Database.
class DataIdentifierTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        Name, Did, Length, DataTypes, ReadSessions, ReadSecurityLevels, WriteSessions, WriteSecurityLevels, Comment,
        ColumnCount
    };

    // Value to sort by: numbers as numbers (so 0x0100 comes before 0xF190).
    static constexpr int SortRole = Qt::UserRole + 1;

    explicit DataIdentifierTableModel(Database *database, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    void namesChanged(int column); // a data type, session or security level was renamed
    // "0x03 Extended, ..." or `none` if there are none.
    QString sessionNames(const QVector<int> &sessionIds, const QString &none) const;
    // "0x01 L1, ..." or "(none needed)".
    QString securityLevelNames(const QVector<int> &securityLevelIds) const;

    Database *m_database = nullptr;
};

#endif // DATAIDENTIFIERTABLEMODEL_H
