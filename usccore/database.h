#ifndef DATABASE_H
#define DATABASE_H

#include "network.h"
#include "securitylevel.h"

#include <QObject>
#include <QStringList>
#include <QVector>

namespace usc {

// Owns all data of one UDScribe document, which describes exactly one ECU.
// It knows nothing about widgets; views find out about changes through its
// Qt signals.
class Database : public QObject
{
    Q_OBJECT

public:
    explicit Database(QObject *parent = nullptr);

    // The network: its active protocol, that protocol's settings, the
    // request/response IDs and the response timing.
    const Network &network() const;
    // Does nothing (and emits nothing) if the network is the same as the stored one.
    void setNetwork(const Network &network);

    const QVector<SecurityLevel> &allSecurityLevels() const;
    QStringList securityLevelNames() const;
    // nullptr if no security level has this id. The pointer is only valid until the next change.
    const SecurityLevel *findSecurityLevel(int id) const;
    // Position of the security level in allSecurityLevels(), or -1 if none has this id.
    int indexOfSecurityLevel(int id) const;

    // Assigns a new unique id to the security level, stores it and returns the id.
    int addSecurityLevel(SecurityLevel securityLevel);
    // Replaces the stored security level that has the same id. Returns false if there is none.
    // Emits nothing if it is the same as the stored one.
    bool updateSecurityLevel(const SecurityLevel &securityLevel);
    // Deletes the security level. Returns false if there is none with this id.
    bool removeSecurityLevel(int id);

    // Throws away everything and stores these instead, each with a new unique id.
    // Used when a file is opened.
    void replaceAll(const Network &newNetwork, QVector<SecurityLevel> newSecurityLevels);

signals:
    void networkChanged();
    // Emitted before and after a security level is inserted at position `index`
    // of allSecurityLevels(). Item models need both to keep their views in sync.
    void securityLevelAboutToBeAdded(int index);
    void securityLevelAdded(int id);
    void securityLevelChanged(int id);
    // Emitted before and after the security level at position `index` is deleted.
    void securityLevelAboutToBeRemoved(int index);
    void securityLevelRemoved(int id);
    // Emitted before and after everything is replaced at once (see replaceAll()).
    void aboutToBeReset();
    void wasReset();

private:
    Network m_network;
    QVector<SecurityLevel> m_securityLevels;
    int m_nextId = 1; // shared by all kinds of objects, so every id is unique
};

} // namespace usc

#endif // DATABASE_H
