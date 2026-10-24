#include "database.h"

namespace usc {

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
    m_network = network;
    emit networkChanged();
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

    m_securityLevels[index] = securityLevel;
    emit securityLevelChanged(securityLevel.id);
    return true;
}

bool Database::removeSecurityLevel(int id)
{
    const int index = indexOfSecurityLevel(id);
    if (index < 0)
        return false;

    emit securityLevelAboutToBeRemoved(index);
    m_securityLevels.removeAt(index);
    emit securityLevelRemoved(id);
    return true;
}

void Database::replaceAll(const Network &newNetwork, QVector<SecurityLevel> newSecurityLevels)
{
    emit aboutToBeReset();
    m_nextId = 1;
    for (SecurityLevel &securityLevel : newSecurityLevels)
        securityLevel.id = m_nextId++;
    m_network = newNetwork;
    m_securityLevels = newSecurityLevels;
    emit wasReset();
}

} // namespace usc
