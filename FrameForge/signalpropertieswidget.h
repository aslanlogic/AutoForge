#ifndef SIGNALPROPERTIESWIDGET_H
#define SIGNALPROPERTIESWIDGET_H

#include <QWidget>

namespace ff {
class Database;
struct Signal;
} // namespace ff

class QTableWidget;

using namespace ff;

// Read-only table of one signal's properties, shown in the right pane.
class SignalPropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SignalPropertiesWidget(QWidget *parent = nullptr);

    // The database is needed to look up the signal's value table.
    void showSignal(const Signal &signal, const Database &database);

private:
    QTableWidget *m_table = nullptr;
};

#endif // SIGNALPROPERTIESWIDGET_H
