#ifndef NODEPROPERTIESWIDGET_H
#define NODEPROPERTIESWIDGET_H

#include <QWidget>

namespace ff {
class Database;
struct Node;
} // namespace ff

class QTableWidget;

using namespace ff;

// Read-only view of one network node, shown in the right pane: its name and
// comment, and the messages it transmits (Tx) and receives (Rx).
class NodePropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NodePropertiesWidget(QWidget *parent = nullptr);

    // The database is needed to look up the node's messages.
    void showNode(const Node &node, const Database &database);

private:
    QTableWidget *m_propertiesTable = nullptr;
    QTableWidget *m_txTable = nullptr;
    QTableWidget *m_rxTable = nullptr;
};

#endif // NODEPROPERTIESWIDGET_H
