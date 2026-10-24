#ifndef DATATYPEPROPERTIESWIDGET_H
#define DATATYPEPROPERTIESWIDGET_H

#include <QWidget>

namespace usc {
class Database;
struct DataType;
} // namespace usc

class QTableWidget;

using namespace usc;

// Read-only view of one data type, shown in the right pane: its properties and
// the DIDs that place it.
class DataTypePropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DataTypePropertiesWidget(QWidget *parent = nullptr);

    // The database is needed to look up the data type's value table.
    void showDataType(const DataType &dataType, const Database &database);

private:
    QTableWidget *m_propertiesTable = nullptr;
    QTableWidget *m_dataIdentifiersTable = nullptr;
};

#endif // DATATYPEPROPERTIESWIDGET_H
