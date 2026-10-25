#ifndef GRAPHWINDOW_H
#define GRAPHWINDOW_H

#include "ffcore/database.h"

#include <QColor>
#include <QPointF>
#include <QVector>
#include <QWidget>

class PlotWidget;
class QCheckBox;
class QComboBox;
class QSettings;
class QToolButton;
class QTreeWidget;
class Tester;

// One signal on the graph and the values it had.
struct PlotSeries
{
    int messageId = 0; // ff Message::id
    int signalId = 0;  // ff Signal::id
    quint32 frameId = 0;
    int startBit = 0;
    ff::Signal signal;
    QString label; // "EngineData.EngineSpeed"
    QColor color;
    bool visible = true;
    bool zoomed = false; // the y axis shows viewLow..viewHigh instead of the signal's range
    double viewLow = 0.0;
    double viewHigh = 1.0;
    QVector<QPointF> points; // x = time in s, y = physical value
};

// Plots bus signals over time in physical units, like CANoe's Graphics window.
//  - Signals: pick up to six, grouped by message (or with a Trace window's Graph button).
//  - Legend: switch a signal on and off, remove it, see its current value.
//  - Axes: one axis per signal, one shared axis, or each signal in its own lane.
//  - Span: the last 10, 30 or 60 s; Pause freezes it. Hovering shows the values at that time.
//  - Zoom: wheel for time, Ctrl+wheel for y, drag to move, double-click or Fit to see all again.
class GraphWindow : public QWidget
{
    Q_OBJECT

public:
    enum class AxisMode { PerSignal, Shared, Stacked };
    static const int MaxSignals = 6;

    explicit GraphWindow(Tester *tester, QWidget *parent = nullptr);

    // Removes all signals.
    void setDatabase(const ff::Database *database);
    // False if it is already plotted, unknown, or six signals are plotted.
    bool addSignal(int messageId, int signalId);
    void removeSignal(int index);
    int signalCount() const { return m_series.size(); }
    const QVector<PlotSeries> &series() const { return m_series; }

    // For a DiagProbe configuration: the signals (by message and signal name), which
    // are shown, the axis mode and the span. Load after setDatabase().
    void saveSettings(QSettings &settings) const;
    void loadSettings(QSettings &settings);

private:
    void onFrame(double time, quint32 id, const QByteArray &payload, bool transmitted);
    void fillSignalMenu();
    void rebuildLegend();
    void refreshLegendValues();

    Tester *m_tester;
    const ff::Database *m_database = nullptr;
    QVector<PlotSeries> m_series;
    PlotWidget *m_plot;
    QTreeWidget *m_legend;
    QToolButton *m_signalsButton;
    QComboBox *m_axisMode;
    QComboBox *m_span;
    int m_presetSpanIndex = 0; // the 10/30/60 s choice that Fit goes back to
    QCheckBox *m_pause;
};

#endif // GRAPHWINDOW_H
