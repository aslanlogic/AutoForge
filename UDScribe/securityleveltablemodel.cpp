#include "securityleveltablemodel.h"
#include "usccore/database.h"

using namespace usc;

SecurityLevelTableModel::SecurityLevelTableModel(Database *database, QObject *parent)
    : QAbstractTableModel(parent)
    , m_database(database)
{
    connect(m_database, &Database::securityLevelAboutToBeAdded, this, [this](int index) {
        beginInsertRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::securityLevelAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::securityLevelChanged, this, [this](int id) {
        const int row = m_database->indexOfSecurityLevel(id);
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    });
    connect(m_database, &Database::securityLevelAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::securityLevelRemoved, this, [this] {
        endRemoveRows();
    });

    // The Allowed Sessions column shows session names.
    connect(m_database, &Database::sessionChanged, this, [this] {
        if (rowCount() > 0)
            emit dataChanged(index(0, AllowedSessions), index(rowCount() - 1, AllowedSessions));
    });

    connect(m_database, &Database::aboutToBeReset, this, &SecurityLevelTableModel::beginResetModel);
    connect(m_database, &Database::wasReset, this, &SecurityLevelTableModel::endResetModel);
}

int SecurityLevelTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_database->allSecurityLevels().size();
}

int SecurityLevelTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant SecurityLevelTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole && role != SortRole))
        return QVariant();

    const SecurityLevel &securityLevel = m_database->allSecurityLevels().at(index.row());
    switch (index.column()) {
    case Name:
        return securityLevel.name;
    case RequestSeed:
        if (role == SortRole)
            return int(securityLevel.requestSeed);
        return subFunctionToText(securityLevel.requestSeed);
    case SendKey:
        if (role == SortRole)
            return int(sendKey(securityLevel.requestSeed));
        return subFunctionToText(sendKey(securityLevel.requestSeed));
    case SeedLength:  return securityLevel.seedLength;
    case KeyLength:   return securityLevel.keyLength;
    case MaxAttempts: return securityLevel.maxAttempts;
    case AllowedSessions: {
        QStringList names;
        for (int sessionId : securityLevel.sessionIds) {
            if (const Session *session = m_database->findSession(sessionId))
                names << subFunctionToText(session->sessionType) + ' ' + session->name;
        }
        return names.join(", ");
    }
    case Comment:     return securityLevel.comment;
    }
    return QVariant();
}

QVariant SecurityLevelTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    static const char *const headers[ColumnCount] = {
        "Name", "Request Seed", "Send Key", "Seed Length", "Key Length", "Max Attempts", "Allowed Sessions", "Comment"};
    return QString(headers[section]);
}
