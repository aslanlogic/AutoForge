#ifndef DATABASE_H
#define DATABASE_H

#include "message.h"
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

    // Throws away everything and stores these instead, each with a new unique id.
    // Used when a file is opened. The signals must come with ids that are unique
    // among them; the messages' signal mappings refer to those ids and are
    // renumbered along with them. Mappings to unknown signals are dropped.
    void replaceAll(QVector<Signal> newSignals, QVector<Message> newMessages);

signals:
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
    // Emitted before and after everything is replaced at once (see replaceAll()).
    void aboutToBeReset();
    void wasReset();

private:
    QVector<Signal> m_signals;
    QVector<Message> m_messages;
    int m_nextId = 1; // shared by signals and messages, so every id is unique
};

#endif // DATABASE_H
