#include "signalpropertieswidget.h"
#include "signal.h"

#include <QHeaderView>
#include <QLocale>
#include <QTableWidget>
#include <QVBoxLayout>

SignalPropertiesWidget::SignalPropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({"Property", "Value"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_table);
}

void SignalPropertiesWidget::showSignal(const Signal &signal)
{
    auto number = [](double value) {
        return QLocale::c().toString(value, 'g', QLocale::FloatingPointShortest);
    };

    const QVector<QPair<QString, QString>> rows = {
        {"Name", signal.name},
        {"Length [Bit]", QString::number(signal.length)},
        {"Byte Order", toString(signal.byteOrder)},
        {"Value Type", toString(signal.valueType)},
        {"Unit", signal.unit},
        {"Init. Value", number(signal.initialValue)},
        {"Factor", number(signal.factor)},
        {"Offset", number(signal.offset)},
        {"Minimum", number(signal.minimum)},
        {"Maximum", number(signal.maximum)},
        {"Comment", signal.comment},
    };

    m_table->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        m_table->setItem(row, 0, new QTableWidgetItem(rows.at(row).first));
        m_table->setItem(row, 1, new QTableWidgetItem(rows.at(row).second));
    }
    m_table->resizeColumnToContents(0);
}
