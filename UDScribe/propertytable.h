#ifndef PROPERTYTABLE_H
#define PROPERTYTABLE_H

#include <QHeaderView>
#include <QPair>
#include <QStringList>
#include <QTableWidget>
#include <QVector>

// Small helpers shared by the read-only views in the right pane.

// A table the user can look at but not edit or select in.
inline QTableWidget *createReadOnlyTable(const QStringList &headers)
{
    auto *table = new QTableWidget(0, headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    return table;
}

// Fills a two-column "Property | Value" table and makes it exactly as tall as
// its rows, so whatever is below it gets the remaining space.
inline void setPropertyRows(QTableWidget *table, const QVector<QPair<QString, QString>> &rows)
{
    table->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        table->setItem(row, 0, new QTableWidgetItem(rows.at(row).first));
        table->setItem(row, 1, new QTableWidgetItem(rows.at(row).second));
    }
    table->resizeColumnToContents(0);
    table->setFixedHeight(table->horizontalHeader()->height()
                          + rows.size() * table->verticalHeader()->defaultSectionSize() + 4);
}

// Fills a table row by row with plain text cells.
inline void setTableRows(QTableWidget *table, const QVector<QStringList> &rows)
{
    table->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        for (int column = 0; column < rows.at(row).size(); ++column)
            table->setItem(row, column, new QTableWidgetItem(rows.at(row).at(column)));
    }
}

#endif // PROPERTYTABLE_H
