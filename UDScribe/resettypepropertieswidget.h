#ifndef RESETTYPEPROPERTIESWIDGET_H
#define RESETTYPEPROPERTIESWIDGET_H

#include <QWidget>

namespace usc {
class Database;
struct ResetType;
} // namespace usc

class QTableWidget;

using namespace usc;

// Read-only view of one reset type, shown in the right pane.
class ResetTypePropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ResetTypePropertiesWidget(QWidget *parent = nullptr);

    // The database is needed for the names of the sessions and security levels.
    void showResetType(const ResetType &resetType, const Database &database);

private:
    QTableWidget *m_propertiesTable = nullptr;
};

#endif // RESETTYPEPROPERTIESWIDGET_H
