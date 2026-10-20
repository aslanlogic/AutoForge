#include "navigationmodel.h"
#include "database.h"

#include <QIcon>

// How rows are identified. Every index carries a number (internalId) that is
// enough to find its parent without extra bookkeeping:
//
//   heading                      0
//   item under a heading         Category + 1                (1 .. CategoryCount)
//   Tx/Rx folder under a node    nodeId * 8 + FolderTag
//   message in a Tx/Rx folder    nodeId * 8 + TxMessageTag / RxMessageTag
//
// Node ids start at 1, so the last two are always 8 or more and never clash
// with the first two. The node id (not its row) is used because rows shift
// when nodes are added or deleted, while ids never change.
namespace {

const quintptr FolderTag = 5;
const quintptr TxMessageTag = 6;
const quintptr RxMessageTag = 7;

quintptr tagOf(quintptr internalId) { return internalId & 7; }
int nodeIdOf(quintptr internalId) { return int(internalId >> 3); }
quintptr makeId(int nodeId, quintptr tag) { return (quintptr(nodeId) << 3) | tag; }

bool isHeading(const QModelIndex &index) { return index.internalId() == 0; }
bool isCategoryItem(const QModelIndex &index)
{
    // Must stay below 8, the smallest number the node rows can have.
    static_assert(NavigationModel::CategoryCount < 8, "too many categories for the internalId scheme");
    return index.internalId() >= 1 && index.internalId() <= NavigationModel::CategoryCount;
}

} // namespace

NavigationModel::NavigationModel(Database *database, QObject *parent)
    : QAbstractItemModel(parent)
    , m_database(database)
{
    connect(m_database, &Database::signalAboutToBeAdded, this, [this](int index) {
        beginInsertRows(categoryIndex(Signals), index, index);
    });
    connect(m_database, &Database::signalAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::signalChanged, this, [this](int id) {
        const QModelIndex changed = index(m_database->indexOfSignal(id), 0, categoryIndex(Signals));
        emit dataChanged(changed, changed);
    });
    connect(m_database, &Database::signalAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(categoryIndex(Signals), index, index);
    });
    connect(m_database, &Database::signalRemoved, this, [this] {
        endRemoveRows();
    });

    // A message also appears under its transmitter's and receivers' folders.
    connect(m_database, &Database::messageAboutToBeAdded, this, [this](int index) {
        beginInsertRows(categoryIndex(Messages), index, index);
    });
    connect(m_database, &Database::messageAdded, this, [this] {
        endInsertRows();
        updateNodeFolders();
    });
    connect(m_database, &Database::messageChanged, this, [this](int id) {
        const QModelIndex changed = index(m_database->indexOfMessage(id), 0, categoryIndex(Messages));
        emit dataChanged(changed, changed);
        updateNodeFolders();
    });
    connect(m_database, &Database::messageAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(categoryIndex(Messages), index, index);
    });
    connect(m_database, &Database::messageRemoved, this, [this] {
        endRemoveRows();
        updateNodeFolders();
    });

    connect(m_database, &Database::nodeAboutToBeAdded, this, [this](int index) {
        beginInsertRows(categoryIndex(NetworkNodes), index, index);
    });
    connect(m_database, &Database::nodeAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::nodeChanged, this, [this](int id) {
        const QModelIndex changed = nodeIndex(id);
        emit dataChanged(changed, changed);
    });
    connect(m_database, &Database::nodeAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(categoryIndex(NetworkNodes), index, index);
    });
    connect(m_database, &Database::nodeRemoved, this, [this](int id) {
        m_txMessages.remove(id);
        m_rxMessages.remove(id);
        endRemoveRows();
    });

    connect(m_database, &Database::valueTableAboutToBeAdded, this, [this](int index) {
        beginInsertRows(categoryIndex(ValueTables), index, index);
    });
    connect(m_database, &Database::valueTableAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::valueTableChanged, this, [this](int id) {
        const QModelIndex changed = index(m_database->indexOfValueTable(id), 0, categoryIndex(ValueTables));
        emit dataChanged(changed, changed);
    });
    connect(m_database, &Database::valueTableAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(categoryIndex(ValueTables), index, index);
    });
    connect(m_database, &Database::valueTableRemoved, this, [this] {
        endRemoveRows();
    });

    connect(m_database, &Database::aboutToBeReset, this, &NavigationModel::beginResetModel);
    connect(m_database, &Database::wasReset, this, [this] {
        rebuildNodeFolders();
        endResetModel();
    });
}

QModelIndex NavigationModel::categoryIndex(Category category) const
{
    return createIndex(category, 0, quintptr(0));
}

QModelIndex NavigationModel::nodeIndex(int nodeId) const
{
    const int row = m_database->indexOfNode(nodeId);
    return row < 0 ? QModelIndex() : createIndex(row, 0, quintptr(NetworkNodes + 1));
}

void NavigationModel::rebuildNodeFolders()
{
    m_txMessages.clear();
    m_rxMessages.clear();
    for (const Node &node : m_database->allNodes()) {
        m_txMessages.insert(node.id, m_database->messagesTransmittedBy(node.id));
        m_rxMessages.insert(node.id, m_database->messagesReceivedBy(node.id));
    }
}

void NavigationModel::updateNodeFolders()
{
    for (const Node &node : m_database->allNodes()) {
        const QModelIndex nodeRow = nodeIndex(node.id);
        for (int folder = 0; folder < 2; ++folder) {
            QVector<int> &cached = (folder == 0) ? m_txMessages[node.id] : m_rxMessages[node.id];
            const QVector<int> current = (folder == 0) ? m_database->messagesTransmittedBy(node.id)
                                                       : m_database->messagesReceivedBy(node.id);
            const QModelIndex folderRow = index(folder, 0, nodeRow);

            if (cached == current) {
                // Same messages; a name may still have changed.
                if (!current.isEmpty())
                    emit dataChanged(index(0, 0, folderRow), index(current.size() - 1, 0, folderRow));
                continue;
            }

            // Simplest correct way to tell the views: remove all old rows, insert all new ones.
            if (!cached.isEmpty()) {
                beginRemoveRows(folderRow, 0, cached.size() - 1);
                cached.clear();
                endRemoveRows();
            }
            if (!current.isEmpty()) {
                beginInsertRows(folderRow, 0, current.size() - 1);
                cached = current;
                endInsertRows();
            }
        }
    }
}

QModelIndex NavigationModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    if (!parent.isValid())
        return createIndex(row, column, quintptr(0));
    if (isHeading(parent))
        return createIndex(row, column, quintptr(parent.row() + 1));

    // Under a node: its two folders.
    if (parent.internalId() == quintptr(NetworkNodes + 1)) {
        const int nodeId = m_database->allNodes().at(parent.row()).id;
        return createIndex(row, column, makeId(nodeId, FolderTag));
    }

    // Under a folder: its messages.
    const int nodeId = nodeIdOf(parent.internalId());
    return createIndex(row, column, makeId(nodeId, parent.row() == 0 ? TxMessageTag : RxMessageTag));
}

QModelIndex NavigationModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || isHeading(child))
        return QModelIndex();
    if (isCategoryItem(child))
        return categoryIndex(static_cast<Category>(child.internalId() - 1));

    const int nodeId = nodeIdOf(child.internalId());
    switch (tagOf(child.internalId())) {
    case FolderTag:    return nodeIndex(nodeId);
    case TxMessageTag: return createIndex(0, 0, makeId(nodeId, FolderTag));
    case RxMessageTag: return createIndex(1, 0, makeId(nodeId, FolderTag));
    }
    return QModelIndex();
}

int NavigationModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return CategoryCount;
    if (parent.column() > 0)
        return 0;

    if (isHeading(parent)) {
        switch (parent.row()) {
        case NetworkNodes: return m_database->allNodes().size();
        case Messages:     return m_database->allMessages().size();
        case Signals:      return m_database->allSignals().size();
        case ValueTables:  return m_database->allValueTables().size();
        }
        return 0;
    }

    // A node has two folders; signals and messages have no children.
    if (isCategoryItem(parent))
        return parent.internalId() == quintptr(NetworkNodes + 1) ? 2 : 0;

    if (tagOf(parent.internalId()) == FolderTag) {
        const int nodeId = nodeIdOf(parent.internalId());
        return (parent.row() == 0 ? m_txMessages : m_rxMessages).value(nodeId).size();
    }
    return 0;
}

int NavigationModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant NavigationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    // Icons are compiled into the program from resources.qrc.
    if (isHeading(index)) {
        static const char *const headings[CategoryCount] = {
            "Network", "Network Nodes", "Messages", "Signals", "Value Tables"};
        static const char *const icons[CategoryCount] = {
            ":/icons/network.svg", ":/icons/nodes.svg", ":/icons/messages.svg", ":/icons/signals.svg",
            ":/icons/valuetables.svg"};
        switch (role) {
        case Qt::DisplayRole:    return QString(headings[index.row()]);
        case Qt::DecorationRole: return QIcon(icons[index.row()]);
        case ItemKindRole:       return Heading;
        }
        return QVariant();
    }

    // Name, icon, kind and id of one row; filled in below.
    QString name;
    QString icon;
    ItemKind kind = Heading;
    int id = 0;

    if (isCategoryItem(index)) {
        switch (index.internalId() - 1) {
        case ValueTables: {
            const ValueTable &valueTable = m_database->allValueTables().at(index.row());
            name = valueTable.name; icon = ":/icons/valuetables.svg"; kind = ValueTableItem; id = valueTable.id;
            break;
        }
        case Signals: {
            const Signal &signal = m_database->allSignals().at(index.row());
            name = signal.name; icon = ":/icons/signals.svg"; kind = SignalItem; id = signal.id;
            break;
        }
        case Messages: {
            const Message &message = m_database->allMessages().at(index.row());
            name = message.name; icon = ":/icons/messages.svg"; kind = MessageItem; id = message.id;
            break;
        }
        case NetworkNodes: {
            const Node &node = m_database->allNodes().at(index.row());
            name = node.name; icon = ":/icons/nodes.svg"; kind = NodeItem; id = node.id;
            break;
        }
        }
    } else if (tagOf(index.internalId()) == FolderTag) {
        const bool tx = (index.row() == 0);
        name = tx ? "Tx Messages" : "Rx Messages";
        icon = tx ? ":/icons/tx.svg" : ":/icons/rx.svg";
        kind = tx ? NodeTxFolder : NodeRxFolder;
        id = nodeIdOf(index.internalId());
    } else {
        const bool tx = (tagOf(index.internalId()) == TxMessageTag);
        const int nodeId = nodeIdOf(index.internalId());
        id = (tx ? m_txMessages : m_rxMessages).value(nodeId).value(index.row());
        const Message *message = m_database->findMessage(id);
        name = message ? message->name : QString();
        icon = ":/icons/messages.svg"; // the same as everywhere else; the folder shows Tx or Rx
        kind = tx ? NodeTxMessage : NodeRxMessage;
    }

    switch (role) {
    case Qt::DisplayRole:    return name;
    case Qt::DecorationRole: return QIcon(icon);
    case ObjectIdRole:       return id;
    case ItemKindRole:       return kind;
    }
    return QVariant();
}
