#include "nodepropertieswidget.h"
#include "database.h"
#include "propertytable.h"

#include <QLabel>
#include <QVBoxLayout>

namespace {

const QStringList MessageColumns = {"Name", "ID", "Type", "Length [Byte]"};

QVector<QStringList> messageRows(const QVector<int> &messageIds, const Database &database)
{
    QVector<QStringList> rows;
    for (int id : messageIds) {
        if (const Message *message = database.findMessage(id)) {
            rows.append({message->name, frameIdToText(message->frameId),
                         toString(message->type), QString::number(message->length)});
        }
    }
    return rows;
}

} // namespace

NodePropertiesWidget::NodePropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    m_propertiesTable = createReadOnlyTable({"Property", "Value"});
    m_txTable = createReadOnlyTable(MessageColumns);
    m_rxTable = createReadOnlyTable(MessageColumns);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_propertiesTable);
    layout->addWidget(new QLabel("Transmitted messages (Tx):"));
    layout->addWidget(m_txTable);
    layout->addWidget(new QLabel("Received messages (Rx):"));
    layout->addWidget(m_rxTable);
}

void NodePropertiesWidget::showNode(const Node &node, const Database &database)
{
    setPropertyRows(m_propertiesTable, {
        {"Name", node.name},
        {"Comment", node.comment},
    });
    setTableRows(m_txTable, messageRows(database.messagesTransmittedBy(node.id), database));
    setTableRows(m_rxTable, messageRows(database.messagesReceivedBy(node.id), database));
}
