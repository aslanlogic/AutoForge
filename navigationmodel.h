#ifndef NAVIGATIONMODEL_H
#define NAVIGATIONMODEL_H

#include <QAbstractItemModel>
#include <QHash>
#include <QVector>

class Database;

// Tree model for the left pane. Shows a fixed heading per category and,
// under each heading, the items of that category taken from the Database.
// Every node also lists the messages it transmits and receives.
//
//   Network
//   Network Nodes
//     <one row per node>
//       Tx Messages
//         <one row per message the node transmits>
//       Rx Messages
//         <one row per message the node receives>
//   Messages
//     <one row per message>
//   Signals
//     <one row per signal>
class NavigationModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Category { Network, NetworkNodes, Messages, Signals, CategoryCount };

    // What a row shows; ask with ItemKindRole.
    enum ItemKind {
        Heading,       // row = Category
        SignalItem,
        MessageItem,
        NodeItem,
        NodeTxFolder,  // "Tx Messages" under a node
        NodeRxFolder,  // "Rx Messages" under a node
        NodeTxMessage, // a message under "Tx Messages"
        NodeRxMessage  // a message under "Rx Messages"
    };

    // The id of the database object shown in that row: Signal::id, Message::id
    // or Node::id. For the Tx/Rx folders it is the id of the node they belong to.
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
    QModelIndex nodeIndex(int nodeId) const;
    void rebuildNodeFolders(); // fills the caches below from the Database, without notifications
    void updateNodeFolders();  // brings the caches up to date and tells the views what changed

    Database *m_database = nullptr;

    // The messages under each node's Tx and Rx folders (node id -> message ids).
    // Kept here rather than asked from the Database each time, so that when a
    // message's transmitter or receivers change, the model can still report the
    // old rows while it tells the views they are removed.
    QHash<int, QVector<int>> m_txMessages;
    QHash<int, QVector<int>> m_rxMessages;
};

#endif // NAVIGATIONMODEL_H
