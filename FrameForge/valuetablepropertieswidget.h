#ifndef VALUETABLEPROPERTIESWIDGET_H
#define VALUETABLEPROPERTIESWIDGET_H

#include <QWidget>

class Database;
struct ValueTable;
class QTableWidget;

// Read-only view of one value table, shown in the right pane: its name and
// comment, its entries, and the signals that use it.
class ValueTablePropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ValueTablePropertiesWidget(QWidget *parent = nullptr);

    // The database is needed to find the signals that use the table.
    void showValueTable(const ValueTable &valueTable, const Database &database);

private:
    QTableWidget *m_propertiesTable = nullptr;
    QTableWidget *m_entriesTable = nullptr;
    QTableWidget *m_signalsTable = nullptr;
};

#endif // VALUETABLEPROPERTIESWIDGET_H
