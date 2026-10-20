#include "messagepropertieswidget.h"
#include "database.h"
#include "messagelayoutwidget.h"

#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QTableWidget *createReadOnlyTable(const QStringList &headers)
{
    auto *table = new QTableWidget(0, headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    return table;
}

} // namespace

MessagePropertiesWidget::MessagePropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    m_propertiesTable = createReadOnlyTable({"Property", "Value"});
    m_signalsTable = createReadOnlyTable({"Name", "Start Bit", "Length [Bit]", "Byte Order"});

    // Same grid as the message dialog's Layout tab, but it cannot be dragged here.
    m_layoutWidget = new MessageLayoutWidget;
    m_layoutWidget->setReadOnly(true);
    auto *layoutScrollArea = new QScrollArea;
    layoutScrollArea->setWidget(m_layoutWidget);

    // Below the properties: the signal list on the left, the layout on the right,
    // with a splitter between them so the user can change how wide each is.
    auto *signalsColumn = new QWidget;
    auto *signalsColumnLayout = new QVBoxLayout(signalsColumn);
    signalsColumnLayout->setContentsMargins(0, 0, 0, 0);
    signalsColumnLayout->addWidget(new QLabel("Signals in this message:"));
    signalsColumnLayout->addWidget(m_signalsTable);

    auto *layoutColumn = new QWidget;
    auto *layoutColumnLayout = new QVBoxLayout(layoutColumn);
    layoutColumnLayout->setContentsMargins(0, 0, 0, 0);
    layoutColumnLayout->addWidget(new QLabel("Layout:"));
    layoutColumnLayout->addWidget(layoutScrollArea);

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(signalsColumn);
    splitter->addWidget(layoutColumn);
    splitter->setChildrenCollapsible(false);
    // When the window is resized, the signal list takes the extra width; the
    // layout keeps its natural width (the whole grid) until the user drags the splitter.
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_propertiesTable);
    layout->addWidget(splitter);
}

void MessagePropertiesWidget::showMessage(const Message &message, const Database &database)
{
    const QString lengthName = isCan(message.type) ? "DLC" : "Length [Byte]";
    const QString cycleTime = isCyclic(message.txMethod) ? QString("%1 ms").arg(message.cycleTime) : QString();
    const QVector<QPair<QString, QString>> rows = {
        {"Name", message.name},
        {"Type", toString(message.type)},
        {"ID", frameIdToText(message.frameId)},
        {lengthName, QString::number(message.length)},
        {"Tx Method", toString(message.txMethod)},
        {"Cycle Time", cycleTime},
        {"Comment", message.comment},
    };

    m_propertiesTable->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        m_propertiesTable->setItem(row, 0, new QTableWidgetItem(rows.at(row).first));
        m_propertiesTable->setItem(row, 1, new QTableWidgetItem(rows.at(row).second));
    }
    m_propertiesTable->resizeColumnToContents(0);
    // Only as tall as its rows, so the signals list gets the remaining space.
    m_propertiesTable->setFixedHeight(m_propertiesTable->horizontalHeader()->height()
                                      + rows.size() * m_propertiesTable->verticalHeader()->defaultSectionSize() + 4);

    m_signalsTable->setRowCount(message.signalMappings.size());
    for (int row = 0; row < message.signalMappings.size(); ++row) {
        const SignalMapping &mapping = message.signalMappings.at(row);
        const Signal *signal = database.findSignal(mapping.signalId);
        if (!signal)
            continue;
        const QStringList cells = {signal->name, QString::number(mapping.startBit),
                                   QString::number(signal->length), toString(signal->byteOrder)};
        for (int column = 0; column < cells.size(); ++column) {
            auto *item = new QTableWidgetItem(cells.at(column));
            // Same colours as in the message dialog's Layout tab.
            item->setBackground(MessageLayoutWidget::colorFor(row));
            item->setForeground(Qt::black);
            m_signalsTable->setItem(row, column, item);
        }
    }

    QHash<int, Signal> signalsById;
    for (const Signal &signal : database.allSignals())
        signalsById.insert(signal.id, signal);
    m_layoutWidget->setLayoutData(message.length, message.signalMappings, signalsById);
}
