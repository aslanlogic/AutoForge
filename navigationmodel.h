#ifndef NAVIGATIONMODEL_H
#define NAVIGATIONMODEL_H

#include <QAbstractItemModel>

class Database;

// Tree model for the left pane. Shows a fixed heading per category and,
// under each heading, the items of that category taken from the Database.
//
//   Network
//   Network Nodes
//   Messages
//     <one row per message>
//   Signals
//     <one row per signal>
class NavigationModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Category { Network, NetworkNodes, Messages, Signals, CategoryCount };

    // Returns the id of the database object shown in that row (Signal::id or Message::id).
    static constexpr int ObjectIdRole = Qt::UserRole + 1;

    explicit NavigationModel(Database *database, QObject *parent = nullptr);

    QModelIndex categoryIndex(Category category) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    Database *m_database = nullptr;
};

#endif // NAVIGATIONMODEL_H
