#ifndef NETWORKPROPERTIESWIDGET_H
#define NETWORKPROPERTIESWIDGET_H

#include <QWidget>

class Database;
class QTableWidget;

// Read-only overview of the whole document, shown when "Network" is clicked.
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
