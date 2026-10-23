#ifndef SECURITYLEVELPROPERTIESWIDGET_H
#define SECURITYLEVELPROPERTIESWIDGET_H

#include <QWidget>

namespace usc {
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

    void showSecurityLevel(const SecurityLevel &securityLevel);

private:
    QTableWidget *m_propertiesTable = nullptr;
};

#endif // SECURITYLEVELPROPERTIESWIDGET_H
