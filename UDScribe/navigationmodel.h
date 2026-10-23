#ifndef NAVIGATIONMODEL_H
#define NAVIGATIONMODEL_H

#include <QAbstractItemModel>

namespace usc {
class Database;
} // namespace usc

using namespace usc;

// Tree model for the left pane. Shows a fixed row per UDS service and, under
// each service, its items taken from the Database.
//
//   0x27 SecurityAccess
//     <one row per security level, e.g. "0x01 Extended">
class NavigationModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Service { SecurityAccess, ServiceCount };

    // What a row shows; ask with ItemKindRole.
    enum ItemKind {
        ServiceItem, // row = Service
        SecurityLevelItem
    };

    // The id of the database object shown in that row, e.g. SecurityLevel::id.
    static constexpr int ObjectIdRole = Qt::UserRole + 1;
    static constexpr int ItemKindRole = Qt::UserRole + 2;

    explicit NavigationModel(Database *database, QObject *parent = nullptr);

    QModelIndex serviceIndex(Service service) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    Database *m_database = nullptr;
};

#endif // NAVIGATIONMODEL_H
