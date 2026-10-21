#ifndef NETWORKPROPERTIESWIDGET_H
#define NETWORKPROPERTIESWIDGET_H

#include <QWidget>

namespace ff {
class Database;
} // namespace ff

class QTableWidget;

using namespace ff;

// Read-only overview of the whole document, shown when "Network" is clicked:
// how many nodes, messages and signals there are, the protocol and its settings.
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
