#ifndef SECURITYLEVELPROPERTIESWIDGET_H
#define SECURITYLEVELPROPERTIESWIDGET_H

#include <QWidget>

namespace usc {
class Database;
struct SecurityLevel;
} // namespace usc

class QTableWidget;

using namespace usc;

// Read-only view of one security level, shown in the right pane.
class SecurityLevelPropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SecurityLevelPropertiesWidget(QWidget *parent = nullptr);

    // The database is needed for the names of the allowed sessions.
    void showSecurityLevel(const SecurityLevel &securityLevel, const Database &database);

private:
    QTableWidget *m_propertiesTable = nullptr;
};

#endif // SECURITYLEVELPROPERTIESWIDGET_H
