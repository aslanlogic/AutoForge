#include "dataidentifiertablemodel.h"
#include "usccore/database.h"

using namespace usc;

DataIdentifierTableModel::DataIdentifierTableModel(Database *database, QObject *parent)
    : QAbstractTableModel(parent)
    , m_database(database)
{
    connect(m_database, &Database::dataIdentifierAboutToBeAdded, this, [this](int index) {
        beginInsertRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::dataIdentifierAdded, this, [this] {
        endInsertRows();
    });
    connect(m_database, &Database::dataIdentifierChanged, this, [this](int id) {
        const int row = m_database->indexOfDataIdentifier(id);
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
    });
    connect(m_database, &Database::dataIdentifierAboutToBeRemoved, this, [this](int index) {
        beginRemoveRows(QModelIndex(), index, index);
    });
    connect(m_database, &Database::dataIdentifierRemoved, this, [this] {
        endRemoveRows();
    });

    // Three columns show names of other objects.
    connect(m_database, &Database::dataTypeChanged, this, [this] { namesChanged(DataTypes); });
    connect(m_database, &Database::sessionChanged, this, [this] { namesChanged(ReadSessions); });
    connect(m_database, &Database::securityLevelChanged, this, [this] { namesChanged(ReadSecurityLevels); });

    connect(m_database, &Database::aboutToBeReset, this, &DataIdentifierTableModel::beginResetModel);
    connect(m_database, &Database::wasReset, this, &DataIdentifierTableModel::endResetModel);
}

void DataIdentifierTableModel::namesChanged(int column)
{
    if (rowCount() > 0)
        emit dataChanged(index(0, column), index(rowCount() - 1, column));
}

int DataIdentifierTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_database->allDataIdentifiers().size();
}

int DataIdentifierTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant DataIdentifierTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole && role != SortRole))
        return QVariant();

    const DataIdentifier &dataIdentifier = m_database->allDataIdentifiers().at(index.row());
    switch (index.column()) {
    case Name:
        return dataIdentifier.name;
    case Did:
        if (role == SortRole)
            return int(dataIdentifier.did);
        return didToText(dataIdentifier.did);
    case Length:
        return dataIdentifier.length;
    case DataTypes: {
        // "FL (Pressure)": the placement's name and, if different, its data type.
        QStringList names;
        for (const DataTypeMapping &mapping : dataIdentifier.mappings) {
            const DataType *dataType = m_database->findDataType(mapping.dataTypeId);
            if (!dataType)
                continue;
            names << (mapping.name == dataType->name ? mapping.name
                                                     : QString("%1 (%2)").arg(mapping.name, dataType->name));
        }
        return names.join(", ");
    }
    case ReadSessions: {
        QStringList names;
        for (int sessionId : dataIdentifier.readSessionIds) {
            if (const Session *session = m_database->findSession(sessionId))
                names << subFunctionToText(session->sessionType) + ' ' + session->name;
        }
        return names.join(", ");
    }
    case ReadSecurityLevels: {
        // Any of them allows reading; none = no security needed.
        QStringList names;
        for (int securityLevelId : dataIdentifier.readSecurityLevelIds) {
            if (const SecurityLevel *securityLevel = m_database->findSecurityLevel(securityLevelId))
                names << subFunctionToText(securityLevel->requestSeed) + ' ' + securityLevel->name;
        }
        return names.isEmpty() ? QString("(none needed)") : names.join(", ");
    }
    case Comment:
        return dataIdentifier.comment;
    }
    return QVariant();
}

QVariant DataIdentifierTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    static const char *const headers[ColumnCount] = {
        "Name", "DID", "Length [Byte]", "Data Types", "Read in Sessions", "Read Security Levels", "Comment"};
    return QString(headers[section]);
}
