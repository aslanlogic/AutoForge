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

void Database::replaceAllSignals(QVector<Signal> newSignals)
{
    emit aboutToBeReset();
    m_nextId = 1;
    for (Signal &signal : newSignals)
        signal.id = m_nextId++;
    m_signals = newSignals;
    emit wasReset();
}
