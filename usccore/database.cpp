#include "database.h"

#include <QHash>

#include <algorithm>

namespace usc {

namespace {

// The ids in `ids` that `newIds` knows, renumbered; the others are dropped.
QVector<int> renumbered(const QVector<int> &ids, const QHash<int, int> &newIds)
{
    QVector<int> result;
    for (int id : ids) {
        if (newIds.contains(id))
            result.append(newIds.value(id));
    }
    return result;
}

Session defaultSession()
{
    Session session;
    session.name = "Default";
    session.sessionType = DefaultSessionType;
    return session;
}

} // namespace

Database::Database(QObject *parent)
    : QObject(parent)
{
    Session session = defaultSession();
    session.id = m_nextId++;
    m_sessions.append(session);
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

const QVector<Session> &Database::allSessions() const
{
    return m_sessions;
}

QStringList Database::sessionNames() const
{
    QStringList names;
    for (const Session &session : m_sessions)
        names << session.name;
    return names;
}

const Session *Database::findSession(int id) const
{
    const int index = indexOfSession(id);
    return index < 0 ? nullptr : &m_sessions.at(index);
}

int Database::indexOfSession(int id) const
{
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions.at(i).id == id)
            return i;
    }
    return -1;
}

int Database::addSession(Session session)
{
    if (session.sessionType == DefaultSessionType)
        return 0; // there is exactly one default session, and it is already there

    session.id = m_nextId++;
    emit sessionAboutToBeAdded(m_sessions.size());
    m_sessions.append(session);
    emit sessionAdded(session.id);
    return session.id;
}

bool Database::updateSession(const Session &session)
{
    const int index = indexOfSession(session.id);
    if (index < 0)
        return false;
    // The default session stays the default session, and no other session becomes one.
    if ((m_sessions.at(index).sessionType == DefaultSessionType) != (session.sessionType == DefaultSessionType))
        return false;
    if (m_sessions.at(index) == session)
        return true; // nothing changed: no signal, so the document is not marked as edited

    m_sessions[index] = session;
    emit sessionChanged(session.id);
    return true;
}

bool Database::removeSession(int id)
{
    const int index = indexOfSession(id);
    if (index < 0 || m_sessions.at(index).sessionType == DefaultSessionType
        || !securityLevelsUsingSession(id).isEmpty() || !resetTypesUsingSession(id).isEmpty())
        return false;

    emit sessionAboutToBeRemoved(index);
    m_sessions.removeAt(index);
    emit sessionRemoved(id);
    return true;
}

QStringList Database::securityLevelsUsingSession(int id) const
{
    QStringList names;
    for (const SecurityLevel &securityLevel : m_securityLevels) {
        if (securityLevel.sessionIds.contains(id))
            names << securityLevel.name;
    }
    return names;
}

QStringList Database::resetTypesUsingSession(int id) const
{
    QStringList names;
    for (const ResetType &resetType : m_resetTypes) {
        if (resetType.sessionIds.contains(id))
            names << resetType.name;
    }
    return names;
}

const QVector<SecurityLevel> &Database::allSecurityLevels() const
{
    return m_securityLevels;
}

QStringList Database::securityLevelNames() const
{
    QStringList names;
    for (const SecurityLevel &securityLevel : m_securityLevels)
        names << securityLevel.name;
    return names;
}

const SecurityLevel *Database::findSecurityLevel(int id) const
{
    const int index = indexOfSecurityLevel(id);
    return index < 0 ? nullptr : &m_securityLevels.at(index);
}

int Database::indexOfSecurityLevel(int id) const
{
    for (int i = 0; i < m_securityLevels.size(); ++i) {
        if (m_securityLevels.at(i).id == id)
            return i;
    }
    return -1;
}

int Database::addSecurityLevel(SecurityLevel securityLevel)
{
    securityLevel.id = m_nextId++;
    emit securityLevelAboutToBeAdded(m_securityLevels.size());
    m_securityLevels.append(securityLevel);
    emit securityLevelAdded(securityLevel.id);
    return securityLevel.id;
}

bool Database::updateSecurityLevel(const SecurityLevel &securityLevel)
{
    const int index = indexOfSecurityLevel(securityLevel.id);
    if (index < 0)
        return false;
    if (m_securityLevels.at(index) == securityLevel)
        return true; // nothing changed: no signal, so the document is not marked as edited

    m_securityLevels[index] = securityLevel;
    emit securityLevelChanged(securityLevel.id);
    return true;
}

bool Database::removeSecurityLevel(int id)
{
    const int index = indexOfSecurityLevel(id);
    if (index < 0 || !resetTypesUsingSecurityLevel(id).isEmpty())
        return false;

    emit securityLevelAboutToBeRemoved(index);
    m_securityLevels.removeAt(index);
    emit securityLevelRemoved(id);
    return true;
}

QStringList Database::resetTypesUsingSecurityLevel(int id) const
{
    QStringList names;
    for (const ResetType &resetType : m_resetTypes) {
        if (resetType.securityLevelIds.contains(id))
            names << resetType.name;
    }
    return names;
}

const QVector<ResetType> &Database::allResetTypes() const
{
    return m_resetTypes;
}

QStringList Database::resetTypeNames() const
{
    QStringList names;
    for (const ResetType &resetType : m_resetTypes)
        names << resetType.name;
    return names;
}

const ResetType *Database::findResetType(int id) const
{
    const int index = indexOfResetType(id);
    return index < 0 ? nullptr : &m_resetTypes.at(index);
}

int Database::indexOfResetType(int id) const
{
    for (int i = 0; i < m_resetTypes.size(); ++i) {
        if (m_resetTypes.at(i).id == id)
            return i;
    }
    return -1;
}

int Database::addResetType(ResetType resetType)
{
    resetType.id = m_nextId++;
    emit resetTypeAboutToBeAdded(m_resetTypes.size());
    m_resetTypes.append(resetType);
    emit resetTypeAdded(resetType.id);
    return resetType.id;
}

bool Database::updateResetType(const ResetType &resetType)
{
    const int index = indexOfResetType(resetType.id);
    if (index < 0)
        return false;
    if (m_resetTypes.at(index) == resetType)
        return true; // nothing changed: no signal, so the document is not marked as edited

    m_resetTypes[index] = resetType;
    emit resetTypeChanged(resetType.id);
    return true;
}

bool Database::removeResetType(int id)
{
    const int index = indexOfResetType(id);
    if (index < 0)
        return false;

    emit resetTypeAboutToBeRemoved(index);
    m_resetTypes.removeAt(index);
    emit resetTypeRemoved(id);
    return true;
}

void Database::replaceAll(const Network &newNetwork, QVector<Session> newSessions,
                          QVector<SecurityLevel> newSecurityLevels, QVector<ResetType> newResetTypes)
{
    // Every document has the default session; it comes first if it has to be added.
    const bool hasDefault = std::any_of(newSessions.cbegin(), newSessions.cend(), [](const Session &session) {
        return session.sessionType == DefaultSessionType;
    });
    if (!hasDefault)
        newSessions.prepend(defaultSession());

    emit aboutToBeReset();
    m_nextId = 1;
    QHash<int, int> newSessionIds; // id given by the caller -> new id
    for (Session &session : newSessions) {
        if (session.id != 0)
            newSessionIds.insert(session.id, m_nextId);
        session.id = m_nextId++;
    }
    QHash<int, int> newSecurityLevelIds; // id given by the caller -> new id
    for (SecurityLevel &securityLevel : newSecurityLevels) {
        if (securityLevel.id != 0)
            newSecurityLevelIds.insert(securityLevel.id, m_nextId);
        securityLevel.id = m_nextId++;
        securityLevel.sessionIds = renumbered(securityLevel.sessionIds, newSessionIds);
    }
    for (ResetType &resetType : newResetTypes) {
        resetType.id = m_nextId++;
        resetType.sessionIds = renumbered(resetType.sessionIds, newSessionIds);
        resetType.securityLevelIds = renumbered(resetType.securityLevelIds, newSecurityLevelIds);
    }
    m_network = newNetwork;
    m_sessions = newSessions;
    m_securityLevels = newSecurityLevels;
    m_resetTypes = newResetTypes;
    emit wasReset();
}

} // namespace usc
