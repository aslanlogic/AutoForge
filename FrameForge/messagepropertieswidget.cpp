#include "messagepropertieswidget.h"
#include "ffcore/database.h"
#include "messagelayoutwidget.h"
#include "propertytable.h"

#include <QLabel>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace ff;

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
    const Network &network = database.network();
    const QString lengthName = isCan(network.protocol) ? "DLC" : "Length [Byte]";
    const QString cycleTime = isCyclic(message.txMethod) ? QString("%1 ms").arg(message.cycleTime) : QString();
    const Node *transmitter = database.findNode(message.transmitterId);
    QStringList receivers;
    for (int receiverId : message.receiverIds) {
        if (const Node *receiver = database.findNode(receiverId))
            receivers << receiver->name;
    }

    // The ID format only means something on CAN and CAN FD.
    const QString idFormat = isCan(network.protocol) ? toString(message.idFormat) : QString("-");

    setPropertyRows(m_propertiesTable, {
        {"Name", message.name},
        {"Protocol", toString(network.protocol)},
        {"ID format", idFormat},
        {"ID", frameIdToText(message.frameId)},
        {lengthName, QString::number(message.length)},
        {"Transmitter", transmitter ? transmitter->name : QString()},
        {"Receivers", receivers.join(", ")},
        {"Tx Method", toString(message.txMethod)},
        {"Cycle Time", cycleTime},
        {"Comment", message.comment},
    });

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
