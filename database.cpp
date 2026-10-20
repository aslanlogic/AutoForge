#include "database.h"

#include <QHash>

Database::Database(QObject *parent)
    : QObject(parent)
{
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

    m_signals[index] = signal;
    emit signalChanged(signal.id);
    return true;
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

    m_messages[index] = message;
    emit messageChanged(message.id);
    return true;
}

void Database::replaceAll(QVector<Signal> newSignals, QVector<Message> newMessages)
{
    emit aboutToBeReset();
    m_nextId = 1;
    QHash<int, int> newSignalIds; // id given by the caller -> new id
    for (Signal &signal : newSignals) {
        newSignalIds.insert(signal.id, m_nextId);
        signal.id = m_nextId++;
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
    }
    m_signals = newSignals;
    m_messages = newMessages;
    emit wasReset();
}
