#ifndef DATABASE_H
#define DATABASE_H

#include "message.h"
#include "network.h"
#include "node.h"
#include "signal.h"
#include "valuetable.h"

#include <QObject>
#include <QStringList>
#include <QVector>

namespace ff {

// Owns all data of one FrameForge document. It knows nothing about widgets;
// views find out about changes through its Qt signals.
class Database : public QObject
{
    Q_OBJECT

public:
    explicit Database(QObject *parent = nullptr);

    // The network: its active protocol and that protocol's settings.
    const Network &network() const;
    void setNetwork(const Network &network);

    const QVector<Signal> &allSignals() const;
    QStringList signalNames() const;
    // nullptr if no signal has this id. The pointer is only valid until the next change.
    const Signal *findSignal(int id) const;
    // Position of the signal in allSignals(), or -1 if no signal has this id.
    int indexOfSignal(int id) const;

    // Assigns a new unique id to the signal, stores it and returns the id.
    int addSignal(Signal signal);
    // Replaces the stored signal that has the same id. Returns false if there is none.
    bool updateSignal(const Signal &signal);
    // Deletes the signal. A signal that is still placed in a message is not
    // deleted: returns false then, and also if there is no signal with this id.
    bool removeSignal(int id);
    // Names of the messages that carry this signal.
    QStringList messagesUsingSignal(int id) const;

    // Messages: same idea as the signal functions above.
    const QVector<Message> &allMessages() const;
    QStringList messageNames() const;
    const Message *findMessage(int id) const;
    int indexOfMessage(int id) const;
    int addMessage(Message message);
    bool updateMessage(const Message &message);
    bool removeMessage(int id);

    // Network nodes: same idea again.
    const QVector<Node> &allNodes() const;
    QStringList nodeNames() const;
    const Node *findNode(int id) const;
    int indexOfNode(int id) const;
    int addNode(Node node);
    bool updateNode(const Node &node);
    // Deletes the node. A node that still transmits or receives a message is not
    // deleted: returns false then, and also if there is no node with this id.
    bool removeNode(int id);
    // Ids of the messages this node transmits / receives, in allMessages() order.
    QVector<int> messagesTransmittedBy(int nodeId) const;
    QVector<int> messagesReceivedBy(int nodeId) const;

    // Value tables: same idea again.
    const QVector<ValueTable> &allValueTables() const;
    QStringList valueTableNames() const;
    const ValueTable *findValueTable(int id) const;
    int indexOfValueTable(int id) const;
    int addValueTable(ValueTable valueTable);
    bool updateValueTable(const ValueTable &valueTable);
    // Deletes the value table. One that a signal still uses is not deleted:
    // returns false then, and also if there is no value table with this id.
    bool removeValueTable(int id);
    // Names of the signals that use this value table.
    QStringList signalsUsingValueTable(int id) const;

    // Throws away everything and stores these instead, each with a new unique id.
    // Used when a file is opened. The signals, nodes and value tables must come
    // with ids that are unique among their kind; the references to them (the
    // messages' signal mappings, transmitter and receivers, the signals' value
    // table) are renumbered along with them. References to unknown ids are dropped.
    void replaceAll(const Network &newNetwork, QVector<Signal> newSignals, QVector<Message> newMessages,
                    QVector<Node> newNodes, QVector<ValueTable> newValueTables);

signals:
    void networkChanged();
    // Emitted before and after a signal is inserted at position `index` of allSignals().
    // Item models need both to keep their views in sync.
    void signalAboutToBeAdded(int index);
    void signalAdded(int id);
    void signalChanged(int id);
    // Emitted before and after the signal at position `index` of allSignals() is deleted.
    void signalAboutToBeRemoved(int index);
    void signalRemoved(int id);
    void messageAboutToBeAdded(int index);
    void messageAdded(int id);
    void messageChanged(int id);
    void messageAboutToBeRemoved(int index);
    void messageRemoved(int id);
    void nodeAboutToBeAdded(int index);
    void nodeAdded(int id);
    void nodeChanged(int id);
    void nodeAboutToBeRemoved(int index);
    void nodeRemoved(int id);
    void valueTableAboutToBeAdded(int index);
    void valueTableAdded(int id);
    void valueTableChanged(int id);
    void valueTableAboutToBeRemoved(int index);
    void valueTableRemoved(int id);
    // Emitted before and after everything is replaced at once (see replaceAll()).
    void aboutToBeReset();
    void wasReset();

private:
    Network m_network;
    QVector<Signal> m_signals;
    QVector<Message> m_messages;
    QVector<Node> m_nodes;
    QVector<ValueTable> m_valueTables;
    int m_nextId = 1; // shared by all kinds of objects, so every id is unique
};

} // namespace ff

#endif // DATABASE_H
