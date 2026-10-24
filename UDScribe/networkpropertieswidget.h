#ifndef NETWORKPROPERTIESWIDGET_H
#define NETWORKPROPERTIESWIDGET_H

#include <QWidget>

namespace usc {
class Database;
} // namespace usc

class QTableWidget;

using namespace usc;

// Read-only overview of the network, shown when "Network" is clicked: the
// request/response IDs, the protocol and its settings.
class NetworkPropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkPropertiesWidget(QWidget *parent = nullptr);

    // filePath: where the document is saved; empty if it has not been saved yet.
    void showNetwork(const Database &database, const QString &filePath);

private:
    QTableWidget *m_propertiesTable = nullptr;
};

#endif // NETWORKPROPERTIESWIDGET_H
