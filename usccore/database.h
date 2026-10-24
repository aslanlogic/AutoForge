#ifndef DATABASE_H
#define DATABASE_H

#include "dataidentifier.h"
#include "datatype.h"
#include "network.h"
#include "resettype.h"
#include "securitylevel.h"
#include "session.h"
#include "valuetable.h"

#include <QObject>
#include <QStringList>
#include <QVector>

namespace usc {

// Owns all data of one UDScribe document, which describes exactly one ECU.
// It knows nothing about widgets; views find out about changes through its
// Qt signals. It always has the default session (0x01).
class Database : public QObject
{
    Q_OBJECT

public:
    // Starts with only the default session.
    explicit Database(QObject *parent = nullptr);

    // The network: its active protocol, that protocol's settings, the
    // request/response IDs and the response timing.
    const Network &network() const;
    // Does nothing (and emits nothing) if the network is the same as the stored one.
    void setNetwork(const Network &network);

    const QVector<Session> &allSessions() const;
    QStringList sessionNames() const;
    // nullptr if no session has this id. The pointer is only valid until the next change.
    const Session *findSession(int id) const;
    // Position of the session in allSessions(), or -1 if none has this id.
    int indexOfSession(int id) const;

    // Assigns a new unique id to the session, stores it and returns the id.
    // A second default session is not added: returns 0 then.
    int addSession(Session session);
    // Replaces the stored session that has the same id. Returns false if there is
    // none, or if it would make the default session another one or another one
    // the default session.
    // Emits nothing if it is the same as the stored one.
    bool updateSession(const Session &session);
    // Deletes the session. The default session and a session that a security level,
    // reset type or DID still uses are not deleted: returns false then, and also if
    // there is no session with this id.
    bool removeSession(int id);
    // Names of the security levels / reset types / DIDs that use this session.
    QStringList securityLevelsUsingSession(int id) const;
    QStringList resetTypesUsingSession(int id) const;
    QStringList dataIdentifiersUsingSession(int id) const;

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
    // Deletes the security level. One that a reset type or DID still needs is not
    // deleted: returns false then, and also if there is none with this id.
    bool removeSecurityLevel(int id);
    // Names of the reset types / DIDs that this security level allows.
    QStringList resetTypesUsingSecurityLevel(int id) const;
    QStringList dataIdentifiersUsingSecurityLevel(int id) const;

    // Value tables: same idea as the security level functions above.
    const QVector<ValueTable> &allValueTables() const;
    QStringList valueTableNames() const;
    const ValueTable *findValueTable(int id) const;
    int indexOfValueTable(int id) const;
    int addValueTable(ValueTable valueTable);
    bool updateValueTable(const ValueTable &valueTable);
    // Deletes the value table. One that a data type still uses is not deleted:
    // returns false then, and also if there is none with this id.
    bool removeValueTable(int id);
    // Names of the data types that use this value table.
    QStringList dataTypesUsingValueTable(int id) const;

    // Data types: same idea again.
    const QVector<DataType> &allDataTypes() const;
    QStringList dataTypeNames() const;
    const DataType *findDataType(int id) const;
    int indexOfDataType(int id) const;
    int addDataType(DataType dataType);
    // Also returns false (and changes nothing) if a DID that places the data type
    // would no longer fit; checkDataTypeChange() says why.
    bool updateDataType(const DataType &dataType);
    // Empty if every DID that places this data type still fits with it as given,
    // otherwise what is wrong.
    QString checkDataTypeChange(const DataType &dataType) const;
    // Deletes the data type. One that a DID still places is not deleted: returns
    // false then, and also if there is none with this id.
    bool removeDataType(int id);
    // Names of the DIDs that place this data type.
    QStringList dataIdentifiersUsingDataType(int id) const;
    // Every data type by id, as checkDataTypeLayout() wants it.
    QHash<int, DataType> dataTypesById() const;

    // DIDs: same idea as the security level functions above.
    const QVector<DataIdentifier> &allDataIdentifiers() const;
    QStringList dataIdentifierNames() const;
    const DataIdentifier *findDataIdentifier(int id) const;
    int indexOfDataIdentifier(int id) const;
    int addDataIdentifier(DataIdentifier dataIdentifier);
    bool updateDataIdentifier(const DataIdentifier &dataIdentifier);
    bool removeDataIdentifier(int id);

    // Reset types: same idea as the security level functions above.
    const QVector<ResetType> &allResetTypes() const;
    QStringList resetTypeNames() const;
    const ResetType *findResetType(int id) const;
    int indexOfResetType(int id) const;
    int addResetType(ResetType resetType);
    bool updateResetType(const ResetType &resetType);
    bool removeResetType(int id);

    // Throws away everything and stores these instead, each with a new unique id.
    // Used when a file is opened. Adds the default session if newSessions has none.
    // The sessions, security levels, value tables and data types must come with ids
    // that are unique among their kind; the references to them are renumbered along
    // with them, unknown ones are dropped (a DID's placement of an unknown data type too).
    void replaceAll(const Network &newNetwork, QVector<Session> newSessions,
                    QVector<SecurityLevel> newSecurityLevels, QVector<ResetType> newResetTypes,
                    QVector<ValueTable> newValueTables, QVector<DataType> newDataTypes,
                    QVector<DataIdentifier> newDataIdentifiers);

signals:
    void networkChanged();
    // Emitted before and after a session is inserted at position `index`
    // of allSessions(). Item models need both to keep their views in sync.
    void sessionAboutToBeAdded(int index);
    void sessionAdded(int id);
    void sessionChanged(int id);
    // Emitted before and after the session at position `index` is deleted.
    void sessionAboutToBeRemoved(int index);
    void sessionRemoved(int id);
    // Emitted before and after a security level is inserted at position `index`
    // of allSecurityLevels(). Item models need both to keep their views in sync.
    void securityLevelAboutToBeAdded(int index);
    void securityLevelAdded(int id);
    void securityLevelChanged(int id);
    // Emitted before and after the security level at position `index` is deleted.
    void securityLevelAboutToBeRemoved(int index);
    void securityLevelRemoved(int id);
    void valueTableAboutToBeAdded(int index);
    void valueTableAdded(int id);
    void valueTableChanged(int id);
    void valueTableAboutToBeRemoved(int index);
    void valueTableRemoved(int id);
    void dataTypeAboutToBeAdded(int index);
    void dataTypeAdded(int id);
    void dataTypeChanged(int id);
    void dataTypeAboutToBeRemoved(int index);
    void dataTypeRemoved(int id);
    void dataIdentifierAboutToBeAdded(int index);
    void dataIdentifierAdded(int id);
    void dataIdentifierChanged(int id);
    void dataIdentifierAboutToBeRemoved(int index);
    void dataIdentifierRemoved(int id);
    void resetTypeAboutToBeAdded(int index);
    void resetTypeAdded(int id);
    void resetTypeChanged(int id);
    void resetTypeAboutToBeRemoved(int index);
    void resetTypeRemoved(int id);
    // Emitted before and after everything is replaced at once (see replaceAll()).
    void aboutToBeReset();
    void wasReset();

private:
    Network m_network;
    QVector<Session> m_sessions;
    QVector<SecurityLevel> m_securityLevels;
    QVector<ResetType> m_resetTypes;
    QVector<ValueTable> m_valueTables;
    QVector<DataType> m_dataTypes;
    QVector<DataIdentifier> m_dataIdentifiers;
    int m_nextId = 1; // shared by all kinds of objects, so every id is unique
};

} // namespace usc

#endif // DATABASE_H
