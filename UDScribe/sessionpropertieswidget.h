#ifndef SESSIONPROPERTIESWIDGET_H
#define SESSIONPROPERTIESWIDGET_H

#include <QWidget>

namespace usc {
struct Session;
} // namespace usc

class QTableWidget;

using namespace usc;

// Read-only view of one session, shown in the right pane.
class SessionPropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SessionPropertiesWidget(QWidget *parent = nullptr);

    void showSession(const Session &session);

private:
    QTableWidget *m_propertiesTable = nullptr;
};

#endif // SESSIONPROPERTIESWIDGET_H
