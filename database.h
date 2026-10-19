#ifndef DATABASE_H
#define DATABASE_H

#include "signal.h"

#include <QObject>
#include <QStringList>
#include <QVector>

// Owns all data of one FrameForge document. It knows nothing about widgets;
// views find out about changes through its Qt signals.
class Database : public QObject
{
    Q_OBJECT

public:
    explicit Database(QObject *parent = nullptr);

    const QVector<Signal> &allSignals() const;
    QStringList signalNames() const;
    // nullptr if no signal has this id. The pointer is only valid until the next change.
    const Signal *findSignal(int id) const;

    // Assigns a new unique id to the signal, stores it and returns the id.
    int addSignal(Signal signal);

signals:
    // Emitted before and after a signal is inserted at position `index` of allSignals().
    // Item models need both to keep their views in sync.
    void signalAboutToBeAdded(int index);
    void signalAdded(int id);

private:
    QVector<Signal> m_signals;
    int m_nextId = 1;
};

#endif // DATABASE_H
