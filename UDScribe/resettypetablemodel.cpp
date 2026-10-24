#include "resettypetablemodel.h"
#include "usccore/database.h"

using namespace usc;

ResetTypeTableModel::ResetTypeTableModel(Database *database, QObject *parent)
    : QAbstractTableModel(parent)
    , m_database(database)
{
    connect(m_database, &Database::resetTypeAboutToBeAdded, this, [this](int index) {
        beginInsertRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::resetTypeAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::resetTypeChanged, this, [this](int id) {
        const int row = m_database->indexOfResetType(id);
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    });
    connect(m_database, &Database::resetTypeAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::resetTypeRemoved, this, [this] {
        endRemoveRows();
    });

    // Two columns show session and security level names.
    connect(m_database, &Database::sessionChanged, this, [this] { namesChanged(AllowedSessions); });
    connect(m_database, &Database::securityLevelChanged, this, [this] { namesChanged(SecurityLevels); });

    connect(m_database, &Database::aboutToBeReset, this, &ResetTypeTableModel::beginResetModel);
    connect(m_database, &Database::wasReset, this, &ResetTypeTableModel::endResetModel);
}

void ResetTypeTableModel::namesChanged(int column)
{
    if (rowCount() > 0)
        emit dataChanged(index(0, column), index(rowCount() - 1, column));
}

int ResetTypeTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_database->allResetTypes().size();
}

int ResetTypeTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ResetTypeTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole && role != SortRole))
        return QVariant();

    const ResetType &resetType = m_database->allResetTypes().at(index.row());
    switch (index.column()) {
    case Name:
        return resetType.name;
    case Type:
        if (role == SortRole)
            return int(resetType.resetType);
        return subFunctionToText(resetType.resetType) + ' ' + resetTypeDescription(resetType.resetType);
    case AllowedSessions: {
        QStringList names;
        for (int sessionId : resetType.sessionIds) {
            if (const Session *session = m_database->findSession(sessionId))
                names << subFunctionToText(session->sessionType) + ' ' + session->name;
        }
        return names.join(", ");
    }
    case SecurityLevels: {
        // Any of them allows the reset; none = no security needed.
        QStringList names;
        for (int securityLevelId : resetType.securityLevelIds) {
            if (const SecurityLevel *securityLevel = m_database->findSecurityLevel(securityLevelId))
                names << subFunctionToText(securityLevel->requestSeed) + ' ' + securityLevel->name;
        }
        return names.isEmpty() ? QString("(none needed)") : names.join(", ");
    }
    case PowerDownTime:
        // Only EnableRapidPowerShutDown has one.
        if (resetType.resetType != EnableRapidPowerShutDown)
            return role == SortRole ? QVariant(-1) : QVariant(QString());
        if (role == SortRole)
            return resetType.powerDownTime;
        return resetType.powerDownTime == PowerDownTimeNotAvailable ? QString("not available")
                                                                    : QString("%1 s").arg(resetType.powerDownTime);
    case Comment:
        return resetType.comment;
    }
    return QVariant();
}

QVariant ResetTypeTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    static const char *const headers[ColumnCount] = {
        "Name", "Reset Type", "Allowed Sessions", "Security Levels", "Power Down Time", "Comment"};
    return QString(headers[section]);
}
