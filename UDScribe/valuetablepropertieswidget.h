#ifndef VALUETABLEPROPERTIESWIDGET_H
#define VALUETABLEPROPERTIESWIDGET_H

#include <QWidget>

namespace usc {
class Database;
struct ValueTable;
} // namespace usc

class QTableWidget;

using namespace usc;

// Read-only view of one value table, shown in the right pane: its name and
// comment, its entries, and the data types that use it.
class ValueTablePropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ValueTablePropertiesWidget(QWidget *parent = nullptr);

    // The database is needed to find the data types that use the table.
    void showValueTable(const ValueTable &valueTable, const Database &database);

private:
    QTableWidget *m_propertiesTable = nullptr;
    QTableWidget *m_entriesTable = nullptr;
    QTableWidget *m_dataTypesTable = nullptr;
};

#endif // VALUETABLEPROPERTIESWIDGET_H
