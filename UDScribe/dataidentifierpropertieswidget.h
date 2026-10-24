#ifndef DATAIDENTIFIERPROPERTIESWIDGET_H
#define DATAIDENTIFIERPROPERTIESWIDGET_H

#include <QWidget>

namespace usc {
class Database;
struct DataIdentifier;
} // namespace usc

class DataIdentifierLayoutWidget;
class QTableWidget;

using namespace usc;

// Read-only view of one DID, shown in the right pane: its properties and,
// below them, the data types placed in its data record as a list and as a layout grid.
class DataIdentifierPropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DataIdentifierPropertiesWidget(QWidget *parent = nullptr);

    // The database is needed for the data types, sessions and security levels.
    void showDataIdentifier(const DataIdentifier &dataIdentifier, const Database &database);

private:
    QTableWidget *m_propertiesTable = nullptr;
    QTableWidget *m_dataTypesTable = nullptr;
    DataIdentifierLayoutWidget *m_layoutWidget = nullptr;
};

#endif // DATAIDENTIFIERPROPERTIESWIDGET_H
