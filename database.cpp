#include "database.h"

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
    for (const Signal &signal : m_signals) {
        if (signal.id == id)
            return &signal;
    }
    return nullptr;
}

int Database::addSignal(Signal signal)
{
    signal.id = m_nextId++;
    emit signalAboutToBeAdded(m_signals.size());
    m_signals.append(signal);
    emit signalAdded(signal.id);
    return signal.id;
}
