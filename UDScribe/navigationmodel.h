#ifndef NAVIGATIONMODEL_H
#define NAVIGATIONMODEL_H

#include "usccore/dataidentifier.h"

#include <QAbstractItemModel>
#include <QVector>

namespace usc {
class Database;
} // namespace usc

using namespace usc;

// Tree model for the left pane. Shows a fixed heading for the network and one
// per UDS service and, under each service, its items taken from the Database.
//
//   Network
//   0x10 DiagnosticSessionControl
//     <one row per session, e.g. "0x01 Default">
//   0x11 ECUReset
//     <one row per reset type, e.g. "0x01 Hard Reset">
//   0x22 ReadDataByIdentifier
//     <one row per DID that can be read, e.g. "0xF190 VIN">
//   0x27 SecurityAccess
//     <one row per security level, e.g. "0x01 Extended">
//   0x2E WriteDataByIdentifier
//     <one row per DID that can be written, e.g. "0xF190 VIN">
//   Value Tables                      (Value Tables, Data Types and Data Identifiers
//     <one row per value table>        are always the last headings)
//   Data Types
//     <one row per data type>
//   Data Identifiers
//     <one row per DID, e.g. "0xF190 VIN">
class NavigationModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    // ValueTables, DataTypes and DataIdentifiers stay last: new services go before them.
    enum Category {
        Network, DiagnosticSessionControl, EcuReset, ReadDataByIdentifier, SecurityAccess, WriteDataByIdentifier,
        ValueTables, DataTypes, DataIdentifiers, CategoryCount
    };

    // What a row shows; ask with ItemKindRole.
    enum ItemKind {
        Heading, // row = Category
        SessionItem,
        ResetTypeItem,
        SecurityLevelItem,
        ValueTableItem,
        DataTypeItem,
        DataIdentifierItem,
        ReadDataIdentifierItem, // a DID under 0x22 ReadDataByIdentifier
        WriteDataIdentifierItem // a DID under 0x2E WriteDataByIdentifier
    };

    // The id of the database object shown in that row: Session::id, ResetType::id,
    // SecurityLevel::id, ValueTable::id, DataType::id or DataIdentifier::id.
    static constexpr int ObjectIdRole = Qt::UserRole + 1;
    static constexpr int ItemKindRole = Qt::UserRole + 2;

    explicit NavigationModel(Database *database, QObject *parent = nullptr);

    QModelIndex categoryIndex(Category category) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    // The DIDs under 0x22 / 0x2E: the readable / writable ones (DataIdentifier::id), in
    // Database order. Kept here rather than asked from the Database each time, so that
    // when a DID stops being readable or writable, the model can still report the old
    // rows while it tells the views they are removed.
    QVector<int> dataIdentifiersWith(DidAccess access) const;
    QVector<int> &cachedDataIdentifiers(DidAccess access);
    const QVector<int> &cachedDataIdentifiers(DidAccess access) const;
    void updateDataIdentifiers(DidAccess access); // brings the cache up to date and tells the views
    void updateAccessDataIdentifiers(); // both of the above

    Database *m_database = nullptr;
    QVector<int> m_readDataIdentifierIds;
    QVector<int> m_writeDataIdentifierIds;
};

#endif // NAVIGATIONMODEL_H
