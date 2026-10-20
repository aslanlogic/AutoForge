#ifndef SIGNALPROPERTIESWIDGET_H
#define SIGNALPROPERTIESWIDGET_H

#include <QWidget>

struct Signal;
class QTableWidget;

// Read-only table of one signal's properties, shown in the right pane.
class SignalPropertiesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SignalPropertiesWidget(QWidget *parent = nullptr);

    void showSignal(const Signal &signal);

private:
    QTableWidget *m_table = nullptr;
};

#endif // SIGNALPROPERTIESWIDGET_H
