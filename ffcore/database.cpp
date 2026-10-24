#include "database.h"

#include <QHash>

namespace ff {

Database::Database(QObject *parent)
    : QObject(parent)
{
}

const Network &Database::network() const
{
    return m_network;
}

void Database::setNetwork(const Network &network)
{
    if (network == m_network)
        return; // nothing changed: no signal, so the document is not marked as edited
    m_network = network;
    emit networkChanged();
}

const QVector<Signal> &Database::allSignals() const
{
    return m_signals;
}

QStringList Database::signalNames() const
{
    QStringList names;
    for (const Signal &signal : m_signals)
        names << signal.name;
    return names;
}

const Signal *Database::findSignal(int id) const
{
    const int index = indexOfSignal(id);
    return index < 0 ? nullptr : &m_signals.at(index);
}

int Database::indexOfSignal(int id) const
{
    for (int i = 0; i < m_signals.size(); ++i) {
        if (m_signals.at(i).id == id)
            return i;
    }
    return -1;
}

int Database::addSignal(Signal signal)
{
    signal.id = m_nextId++;
    emit signalAboutToBeAdded(m_signals.size());
    m_signals.append(signal);
    emit signalAdded(signal.id);
    return signal.id;
}

bool Database::updateSignal(const Signal &signal)
{
    const int index = indexOfSignal(signal.id);
    if (index < 0)
        return false;
    if (m_signals.at(index) == signal)
        return true; // nothing changed: no signal, so the document is not marked as edited

    m_signals[index] = signal;
    emit signalChanged(signal.id);
    return true;
}

bool Database::removeSignal(int id)
{
    const int index = indexOfSignal(id);
    if (index < 0 || !messagesUsingSignal(id).isEmpty())
        return false;

    emit signalAboutToBeRemoved(index);
    m_signals.removeAt(index);
    emit signalRemoved(id);
    return true;
}

QStringList Database::messagesUsingSignal(int id) const
{
    QStringList names;
    for (const Message &message : m_messages) {
        for (const SignalMapping &mapping : message.signalMappings) {
            if (mapping.signalId == id) {
                names << message.name;
                break;
            }
        }
    }
    return names;
}

const QVector<Message> &Database::allMessages() const
{
    return m_messages;
}

QStringList Database::messageNames() const
{
    QStringList names;
    for (const Message &message : m_messages)
        names << message.name;
    return names;
}

const Message *Database::findMessage(int id) const
{
    const int index = indexOfMessage(id);
    return index < 0 ? nullptr : &m_messages.at(index);
}

int Database::indexOfMessage(int id) const
{
    for (int i = 0; i < m_messages.size(); ++i) {
        if (m_messages.at(i).id == id)
            return i;
    }
    return -1;
}

int Database::addMessage(Message message)
{
    message.id = m_nextId++;
    emit messageAboutToBeAdded(m_messages.size());
    m_messages.append(message);
    emit messageAdded(message.id);
    return message.id;
}

bool Database::updateMessage(const Message &message)
{
    const int index = indexOfMessage(message.id);
    if (index < 0)
        return false;
    if (m_messages.at(index) == message)
        return true; // nothing changed: no signal, so the document is not marked as edited

    m_messages[index] = message;
    emit messageChanged(message.id);
    return true;
}

bool Database::removeMessage(int id)
{
    const int index = indexOfMessage(id);
    if (index < 0)
        return false;

    emit messageAboutToBeRemoved(index);
    m_messages.removeAt(index);
    emit messageRemoved(id);
    return true;
}

const QVector<Node> &Database::allNodes() const
{
    return m_nodes;
}

QStringList Database::nodeNames() const
{
    QStringList names;
    for (const Node &node : m_nodes)
        names << node.name;
    return names;
}

const Node *Database::findNode(int id) const
{
    const int index = indexOfNode(id);
    return index < 0 ? nullptr : &m_nodes.at(index);
}

int Database::indexOfNode(int id) const
{
    for (int i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes.at(i).id == id)
            return i;
    }
    return -1;
}

int Database::addNode(Node node)
{
    node.id = m_nextId++;
    emit nodeAboutToBeAdded(m_nodes.size());
    m_nodes.append(node);
    emit nodeAdded(node.id);
    return node.id;
}

bool Database::updateNode(const Node &node)
{
    const int index = indexOfNode(node.id);
    if (index < 0)
        return false;
    if (m_nodes.at(index) == node)
        return true; // nothing changed: no signal, so the document is not marked as edited

    m_nodes[index] = node;
    emit nodeChanged(node.id);
    return true;
}

bool Database::removeNode(int id)
{
    const int index = indexOfNode(id);
    if (index < 0 || !messagesTransmittedBy(id).isEmpty() || !messagesReceivedBy(id).isEmpty())
        return false;

    emit nodeAboutToBeRemoved(index);
    m_nodes.removeAt(index);
    emit nodeRemoved(id);
    return true;
}

QVector<int> Database::messagesTransmittedBy(int nodeId) const
{
    QVector<int> ids;
    for (const Message &message : m_messages) {
        if (message.transmitterId == nodeId)
            ids.append(message.id);
    }
    return ids;
}

QVector<int> Database::messagesReceivedBy(int nodeId) const
{
    QVector<int> ids;
    for (const Message &message : m_messages) {
        if (message.receiverIds.contains(nodeId))
            ids.append(message.id);
    }
    return ids;
}

const QVector<ValueTable> &Database::allValueTables() const
{
    return m_valueTables;
}

QStringList Database::valueTableNames() const
{
    QStringList names;
    for (const ValueTable &valueTable : m_valueTables)
        names << valueTable.name;
    return names;
}

const ValueTable *Database::findValueTable(int id) const
{
    const int index = indexOfValueTable(id);
    return index < 0 ? nullptr : &m_valueTables.at(index);
}

int Database::indexOfValueTable(int id) const
{
    for (int i = 0; i < m_valueTables.size(); ++i) {
        if (m_valueTables.at(i).id == id)
            return i;
    }
    return -1;
}

int Database::addValueTable(ValueTable valueTable)
{
    valueTable.id = m_nextId++;
    emit valueTableAboutToBeAdded(m_valueTables.size());
    m_valueTables.append(valueTable);
    emit valueTableAdded(valueTable.id);
    return valueTable.id;
}

bool Database::updateValueTable(const ValueTable &valueTable)
{
    const int index = indexOfValueTable(valueTable.id);
    if (index < 0)
        return false;
    if (m_valueTables.at(index) == valueTable)
        return true; // nothing changed: no signal, so the document is not marked as edited

    m_valueTables[index] = valueTable;
    emit valueTableChanged(valueTable.id);
    return true;
}

bool Database::removeValueTable(int id)
{
    const int index = indexOfValueTable(id);
    if (index < 0 || !signalsUsingValueTable(id).isEmpty())
        return false;

    emit valueTableAboutToBeRemoved(index);
    m_valueTables.removeAt(index);
    emit valueTableRemoved(id);
    return true;
}

QStringList Database::signalsUsingValueTable(int id) const
{
    QStringList names;
    for (const Signal &signal : m_signals) {
        if (signal.valueTableId == id)
            names << signal.name;
    }
    return names;
}

void Database::replaceAll(const Network &newNetwork, QVector<Signal> newSignals, QVector<Message> newMessages,
                          QVector<Node> newNodes, QVector<ValueTable> newValueTables)
{
    emit aboutToBeReset();
    m_nextId = 1;
    QHash<int, int> newValueTableIds; // id given by the caller -> new id
    for (ValueTable &valueTable : newValueTables) {
        newValueTableIds.insert(valueTable.id, m_nextId);
        valueTable.id = m_nextId++;
    }
    QHash<int, int> newSignalIds;
    for (Signal &signal : newSignals) {
        newSignalIds.insert(signal.id, m_nextId);
        signal.id = m_nextId++;
        signal.valueTableId = newValueTableIds.value(signal.valueTableId, 0);
    }
    QHash<int, int> newNodeIds;
    for (Node &node : newNodes) {
        newNodeIds.insert(node.id, m_nextId);
        node.id = m_nextId++;
    }
    for (Message &message : newMessages) {
        message.id = m_nextId++;
        QVector<SignalMapping> mappings;
        for (SignalMapping mapping : message.signalMappings) {
            if (!newSignalIds.contains(mapping.signalId))
                continue;
            mapping.signalId = newSignalIds.value(mapping.signalId);
            mappings.append(mapping);
        }
        message.signalMappings = mappings;

        message.transmitterId = newNodeIds.value(message.transmitterId, 0);
        QVector<int> receiverIds;
        for (int receiverId : message.receiverIds) {
            if (newNodeIds.contains(receiverId))
                receiverIds.append(newNodeIds.value(receiverId));
        }
        message.receiverIds = receiverIds;
    }
    m_network = newNetwork;
    m_signals = newSignals;
    m_messages = newMessages;
    m_nodes = newNodes;
    m_valueTables = newValueTables;
    emit wasReset();
}

} // namespace ff
