#ifndef MESSAGEPROPERTIESWIDGET_H
#define MESSAGEPROPERTIESWIDGET_H

#include <QWidget>

namespace ff {
class Database;
struct Message;
} // namespace ff

class MessageLayoutWidget;
class QTableWidget;

using namespace ff;

// Read-only view of one message, shown in the right pane: its properties and,
// below them, the signals it carries as a list and as a layout grid.
class MessagePropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MessagePropertiesWidget(QWidget *parent = nullptr);

    // The database is needed to look up the message's signals.
    void showMessage(const Message &message, const Database &database);

private:
    QTableWidget *m_propertiesTable = nullptr;
    QTableWidget *m_signalsTable = nullptr;
    MessageLayoutWidget *m_layoutWidget = nullptr;
};

#endif // MESSAGEPROPERTIESWIDGET_H
