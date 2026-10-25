#include "graphwindow.h"
#include "decoding.h"
#include "tester.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>

#include <cmath>
#include <functional>

namespace {

const double HistoryLength = 120.0; // s of values kept per signal

const QColor Colors[GraphWindow::MaxSignals] = {
    QColor(31, 119, 180), QColor(214, 39, 40), QColor(44, 160, 44),
    QColor(255, 127, 14), QColor(148, 103, 189), QColor(23, 190, 207),
};

// The y range of a series: the signal's minimum to maximum, or else what it has shown.
QPair<double, double> rangeOf(const PlotSeries &series)
{
    double low = series.signal.minimum;
    double high = series.signal.maximum;
    if (!(high > low)) {
        low = high = series.points.isEmpty() ? 0.0 : series.points.first().y();
        for (const QPointF &point : series.points) {
            low = qMin(low, point.y());
            high = qMax(high, point.y());
        }
        if (high - low < 1e-9) {
            low -= 1;
            high += 1;
        }
    }
    return {low, high};
}

// The value a series had at a time: its last value at or before it.
bool valueAt(const PlotSeries &series, double time, double *value)
{
    auto it = std::upper_bound(series.points.begin(), series.points.end(), time,
                               [](double t, const QPointF &point) { return t < point.x(); });
    if (it == series.points.begin())
        return false;
    *value = (it - 1)->y();
    return true;
}

QString valueText(const PlotSeries &series, double value, const ff::Database *database)
{
    QString text = formatNumber(std::round(value * 1000.0) / 1000.0);
    if (!series.signal.unit.isEmpty())
        text += ' ' + series.signal.unit;
    if (const ff::ValueTable *table = database ? database->findValueTable(series.signal.valueTableId) : nullptr) {
        const qint64 raw = qint64(std::llround((value - series.signal.offset) / (series.signal.factor != 0 ? series.signal.factor : 1)));
        for (const ff::ValueTable::Entry &entry : table->entries) {
            if (entry.value == raw)
                return text + " \"" + entry.description + '"';
        }
    }
    return text;
}

} // namespace

// The drawing area of the Graphics window.
//  - Wheel: zoom the time axis around the cursor.
//  - Ctrl+wheel: zoom the y axis: over one signal's axis (or lane) only that signal, else all.
//  - Drag: move the curves; dragging back in time pauses the graph.
//  - Double-click: fit everything again and follow the measurement.
class PlotWidget : public QWidget
{
public:
    PlotWidget(QVector<PlotSeries> *series, QWidget *parent = nullptr)
        : QWidget(parent), m_series(series)
    {
        setMouseTracking(true);
        setMinimumSize(300, 160);
        setAutoFillBackground(true);
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, Qt::white);
        setPalette(palette);
        setToolTip("Wheel: zoom time · Ctrl+wheel: zoom Y (over an axis: only that signal)\n"
                   "Drag: move · Double-click: fit");
    }

    GraphWindow::AxisMode axisMode = GraphWindow::AxisMode::PerSignal;
    double span = 10.0;   // s
    double endTime = 0.0; // s; the right edge
    const ff::Database *database = nullptr;
    bool live = true; // the right edge follows the measurement; false while paused
    std::function<void()> onPan;        // the user moved the time axis: the graph pauses
    std::function<void()> onSpanChange; // the user zoomed the time axis
    std::function<void()> onFit;        // the user asked to see everything again

    // Forgets every y zoom.
    void fitY()
    {
        for (PlotSeries &series : *m_series)
            series.zoomed = false;
        m_sharedZoomed = false;
        update();
    }

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        const QPointF position = event->position();
        const double factor = event->angleDelta().y() > 0 ? 0.8 : 1.25;
        const QRectF plot = plotRect();
        if (event->modifiers() & Qt::ControlModifier) {
            const QVector<int> targets = targetsAt(position);
            QVector<QPair<double, double>> ranges = currentRanges();
            for (int index : targets) {
                const QRectF lane = laneOf(index, plot);
                const auto &range = ranges[index];
                const double fraction = qBound(0.0, (lane.bottom() - position.y()) / lane.height(), 1.0);
                const double value = range.first + fraction * (range.second - range.first);
                setRange(index, value - (value - range.first) * factor, value + (range.second - value) * factor);
            }
        } else {
            const double newSpan = qBound(0.05, span * factor, 600.0);
            if (live) {
                span = newSpan; // keep the right edge at "now"
            } else {
                const double fraction = qBound(0.0, (position.x() - plot.left()) / plot.width(), 1.0);
                const double time = endTime - span + fraction * span;
                endTime = time + (1 - fraction) * newSpan;
                span = newSpan;
            }
            if (onSpanChange)
                onSpanChange();
        }
        update();
        event->accept();
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton)
            return;
        m_dragStart = event->pos();
        m_dragTargets = targetsAt(event->pos());
        m_dragRanges = currentRanges();
        m_dragEndTime = endTime;
        m_dragging = true;
        setCursor(Qt::ClosedHandCursor);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        m_hoverX = event->pos().x();
        if (m_dragging) {
            const QRectF plot = plotRect();
            const QPointF delta = event->pos() - m_dragStart;
            if (std::abs(delta.x()) > 2) {
                endTime = m_dragEndTime - delta.x() / plot.width() * span;
                if (onPan)
                    onPan();
            }
            for (int index : m_dragTargets) {
                const auto &range = m_dragRanges[index];
                const double shift = delta.y() / laneOf(index, plot).height() * (range.second - range.first);
                setRange(index, range.first + shift, range.second + shift);
            }
        }
        update();
    }

    void mouseReleaseEvent(QMouseEvent *) override
    {
        m_dragging = false;
        unsetCursor();
    }

    void mouseDoubleClickEvent(QMouseEvent *) override
    {
        fitY();
        if (onFit)
            onFit();
    }

    void leaveEvent(QEvent *) override
    {
        m_hoverX = -1;
        update();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QVector<int> visible = visibleIndexes();
        const QRectF plot = plotRect();
        if (plot.width() < 20 || plot.height() < 20)
            return;
        const double startTime = endTime - span;
        auto xOf = [&](double t) { return plot.left() + (t - startTime) / span * plot.width(); };

        // Time grid and labels, in steps of 1, 2 or 5 times a power of ten.
        const double rough = span / 8.0;
        const double power = std::pow(10.0, std::floor(std::log10(rough)));
        const double step = rough / power < 2 ? power : rough / power < 5 ? 2 * power : 5 * power;
        const int decimals = step >= 1 ? 0 : int(std::ceil(-std::log10(step) - 1e-9));
        for (double t = std::ceil(startTime / step) * step; t <= endTime + 1e-9; t += step) {
            painter.setPen(QColor(225, 225, 225));
            painter.drawLine(QPointF(xOf(t), plot.top()), QPointF(xOf(t), plot.bottom()));
            painter.setPen(Qt::darkGray);
            if (t >= -1e-9 && xOf(t) > plot.left() + 15)
                painter.drawText(QRectF(xOf(t) - 35, plot.bottom() + 2, 70, 18), Qt::AlignCenter,
                                 QString::number(qMax(0.0, t), 'f', decimals) + " s");
        }
        painter.setPen(Qt::darkGray);
        painter.drawRect(plot);

        if (visible.isEmpty()) {
            painter.drawText(plot, Qt::AlignCenter, "Add a signal with Signals ▾ or a Trace window's Graph button.");
            return;
        }

        const QVector<QPair<double, double>> ranges = currentRanges();
        auto yOf = [&](int index, double value) {
            const QRectF lane = laneOf(index, plot);
            const auto &range = ranges[index];
            return lane.bottom() - (value - range.first) / (range.second - range.first) * lane.height();
        };

        // Axes.
        for (int position = 0; position < visible.size(); ++position) {
            const int index = visible[position];
            if (axisMode == GraphWindow::AxisMode::Shared && position > 0)
                break;
            const QRectF lane = laneOf(index, plot);
            const double x = axisMode == GraphWindow::AxisMode::PerSignal ? plot.left() - AxisWidth * position - 2
                                                                           : plot.left() - 2;
            painter.setPen(axisMode == GraphWindow::AxisMode::Shared ? QColor(Qt::darkGray) : (*m_series)[index].color);
            painter.drawLine(QPointF(x, lane.top()), QPointF(x, lane.bottom()));
            for (int tick = 0; tick <= 4; ++tick) {
                if (axisMode == GraphWindow::AxisMode::Stacked && (tick == 1 || tick == 3))
                    continue;
                const double value = ranges[index].first + (ranges[index].second - ranges[index].first) * tick / 4.0;
                const double y = yOf(index, value);
                painter.drawLine(QPointF(x - 3, y), QPointF(x, y));
                // In lanes, the top and bottom labels move inwards so neighbouring lanes do not overlap.
                double shift = 0;
                if (axisMode == GraphWindow::AxisMode::Stacked)
                    shift = tick == 4 ? 6 : tick == 0 ? -6 : 0;
                painter.drawText(QRectF(x - AxisWidth, y - 8 + shift, AxisWidth - 5, 16),
                                 Qt::AlignRight | Qt::AlignVCenter, QString::number(value, 'g', 5));
            }
            if (axisMode == GraphWindow::AxisMode::Stacked && position > 0) {
                painter.setPen(QColor(210, 210, 210));
                painter.drawLine(QPointF(plot.left(), lane.top() - 4), QPointF(plot.right(), lane.top() - 4));
            }
        }

        // The curves.
        painter.save();
        painter.setClipRect(plot);
        for (int index : visible) {
            const QVector<QPointF> &points = (*m_series)[index].points;
            QPolygonF line;
            auto first = std::lower_bound(points.begin(), points.end(), startTime,
                                          [](const QPointF &point, double t) { return point.x() < t; });
            if (first != points.begin())
                --first; // one more to the left, so the line starts at the edge
            for (auto it = first; it != points.end(); ++it) {
                line << QPointF(xOf(it->x()), yOf(index, it->y()));
                if (it->x() > endTime)
                    break; // one past the right edge is enough
            }
            painter.setPen(QPen((*m_series)[index].color, 1.6));
            painter.drawPolyline(line);
        }
        painter.restore();

        // The cursor: a line and every value at that time.
        if (m_hoverX >= plot.left() && m_hoverX <= plot.right() && !m_dragging) {
            const double t = startTime + (m_hoverX - plot.left()) / plot.width() * span;
            painter.setPen(QPen(Qt::black, 1, Qt::DashLine));
            painter.drawLine(QPointF(m_hoverX, plot.top()), QPointF(m_hoverX, plot.bottom()));
            QStringList lines = {QString("t = %1 s").arg(t, 0, 'f', 3)};
            QVector<QColor> colors = {Qt::black};
            for (int index : visible) {
                double value;
                if (valueAt((*m_series)[index], t, &value)) {
                    lines << (*m_series)[index].signal.name + ": " + valueText((*m_series)[index], value, database);
                    colors << (*m_series)[index].color;
                }
            }
            const QFontMetrics metrics(font());
            int boxWidth = 0;
            for (const QString &line : lines)
                boxWidth = qMax(boxWidth, metrics.horizontalAdvance(line));
            boxWidth += 12;
            const int lineHeight = metrics.height();
            double boxX = m_hoverX + 8;
            if (boxX + boxWidth > plot.right())
                boxX = m_hoverX - 8 - boxWidth;
            const QRectF box(boxX, plot.top() + 6, boxWidth, lineHeight * lines.size() + 8);
            painter.setPen(QColor(180, 180, 180));
            painter.setBrush(QColor(255, 255, 255, 230));
            painter.drawRect(box);
            for (int i = 0; i < lines.size(); ++i) {
                painter.setPen(colors[i]);
                painter.drawText(QPointF(box.left() + 6, box.top() + 4 + metrics.ascent() + i * lineHeight), lines[i]);
            }
        }
    }

private:
    static const int AxisWidth = 52;

    QVector<int> visibleIndexes() const
    {
        QVector<int> visible;
        for (int i = 0; i < m_series->size(); ++i) {
            if ((*m_series)[i].visible)
                visible.append(i);
        }
        return visible;
    }

    QRectF plotRect() const
    {
        const int axisCount = axisMode == GraphWindow::AxisMode::PerSignal ? qMax(1, int(visibleIndexes().size())) : 1;
        return QRectF(AxisWidth * axisCount + 6, 8, width() - AxisWidth * axisCount - 26, height() - 30);
    }

    // Where series `index` is drawn: the whole plot, or its lane.
    QRectF laneOf(int index, const QRectF &plot) const
    {
        const QVector<int> visible = visibleIndexes();
        if (axisMode != GraphWindow::AxisMode::Stacked || !visible.contains(index))
            return plot.adjusted(0, 4, 0, -4);
        const double laneHeight = plot.height() / visible.size();
        return QRectF(plot.left(), plot.top() + visible.indexOf(index) * laneHeight, plot.width(), laneHeight)
            .adjusted(0, 4, 0, -4);
    }

    // The y range of every series (by index into *m_series), with its zoom.
    QVector<QPair<double, double>> currentRanges() const
    {
        QVector<QPair<double, double>> ranges;
        for (const PlotSeries &series : *m_series)
            ranges.append(series.zoomed ? qMakePair(series.viewLow, series.viewHigh) : rangeOf(series));
        if (axisMode == GraphWindow::AxisMode::Shared) {
            QPair<double, double> all(0, 0);
            bool first = true;
            for (int index : visibleIndexes()) {
                const auto range = rangeOf((*m_series)[index]);
                all = first ? range : qMakePair(qMin(all.first, range.first), qMax(all.second, range.second));
                first = false;
            }
            if (m_sharedZoomed)
                all = qMakePair(m_sharedLow, m_sharedHigh);
            for (auto &range : ranges)
                range = all;
        }
        return ranges;
    }

    void setRange(int index, double low, double high)
    {
        if (high - low < 1e-9)
            return;
        if (axisMode == GraphWindow::AxisMode::Shared) {
            m_sharedZoomed = true;
            m_sharedLow = low;
            m_sharedHigh = high;
            return;
        }
        PlotSeries &series = (*m_series)[index];
        series.zoomed = true;
        series.viewLow = low;
        series.viewHigh = high;
    }

    // The series a y zoom or move at this point acts on: the one whose axis or lane it is over, else all.
    QVector<int> targetsAt(const QPointF &position) const
    {
        const QVector<int> visible = visibleIndexes();
        const QRectF plot = plotRect();
        if (visible.isEmpty())
            return {};
        if (axisMode == GraphWindow::AxisMode::Shared)
            return {visible.first()};
        if (axisMode == GraphWindow::AxisMode::Stacked) {
            for (int index : visible) {
                if (laneOf(index, plot).adjusted(0, -4, 0, 4).contains(QPointF(plot.left() + 1, position.y())))
                    return {index};
            }
            return {};
        }
        if (position.x() < plot.left()) {
            const int axis = int((plot.left() - position.x()) / AxisWidth);
            return axis < visible.size() ? QVector<int>{visible[axis]} : QVector<int>{};
        }
        return visible;
    }

    QVector<PlotSeries> *m_series;
    double m_hoverX = -1;
    bool m_sharedZoomed = false;
    double m_sharedLow = 0, m_sharedHigh = 1;
    bool m_dragging = false;
    QPoint m_dragStart;
    QVector<int> m_dragTargets;
    QVector<QPair<double, double>> m_dragRanges;
    double m_dragEndTime = 0;
};

GraphWindow::GraphWindow(Tester *tester, QWidget *parent)
    : QWidget(parent)
    , m_tester(tester)
{
    m_plot = new PlotWidget(&m_series);

    m_signalsButton = new QToolButton;
    m_signalsButton->setText("Signals");
    m_signalsButton->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(m_signalsButton);
    connect(menu, &QMenu::aboutToShow, this, &GraphWindow::fillSignalMenu);
    m_signalsButton->setMenu(menu);

    m_axisMode = new QComboBox;
    m_axisMode->addItem("One axis per signal");
    m_axisMode->addItem("Shared axis");
    m_axisMode->addItem("Stacked lanes");
    connect(m_axisMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_plot->axisMode = AxisMode(index);
        m_plot->update();
    });

    m_span = new QComboBox;
    m_span->addItem("10 s", 10.0);
    m_span->addItem("30 s", 30.0);
    m_span->addItem("60 s", 60.0);
    connect(m_span, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 3)
            m_presetSpanIndex = index;
        m_plot->span = m_span->currentData().toDouble();
        m_plot->update();
    });

    m_pause = new QCheckBox("Pause");
    connect(m_pause, &QCheckBox::toggled, this, [this](bool paused) { m_plot->live = !paused; });

    auto *fitButton = new QToolButton;
    fitButton->setText("Fit");
    fitButton->setToolTip("Undo every zoom and follow the measurement again (or double-click the graph)");
    auto fit = [this] {
        m_plot->fitY();
        m_pause->setChecked(false);
        // Back to the chosen span; a zoomed span is the extra item at the end.
        QSignalBlocker blocker(m_span);
        if (m_span->count() > 3)
            m_span->removeItem(3);
        m_span->setCurrentIndex(m_presetSpanIndex);
        m_plot->span = m_span->currentData().toDouble();
        if (m_tester->isRunning())
            m_plot->endTime = qMax(m_plot->span, m_tester->time());
        m_plot->update();
    };
    connect(fitButton, &QToolButton::clicked, this, fit);
    m_plot->onFit = fit;
    m_plot->onPan = [this] { m_pause->setChecked(true); };
    m_plot->onSpanChange = [this] {
        // Show the zoomed span in the span box, as an extra item.
        QSignalBlocker blocker(m_span);
        if (m_span->count() <= 3)
            m_span->addItem(QString());
        m_span->setItemText(3, QString::number(m_plot->span, 'g', 3) + " s (zoomed)");
        m_span->setItemData(3, m_plot->span);
        m_span->setCurrentIndex(3);
    };

    auto *bar = new QHBoxLayout;
    bar->addWidget(m_signalsButton);
    bar->addWidget(new QLabel("Axes:"));
    bar->addWidget(m_axisMode);
    bar->addWidget(new QLabel("Span:"));
    bar->addWidget(m_span);
    bar->addWidget(m_pause);
    bar->addWidget(fitButton);
    bar->addStretch();

    m_legend = new QTreeWidget;
    m_legend->setColumnCount(3);
    m_legend->setHeaderLabels({"Signal", "Value", ""});
    m_legend->setRootIsDecorated(false);
    m_legend->header()->setStretchLastSection(false);
    m_legend->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_legend->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_legend->setColumnWidth(2, 28);
    connect(m_legend, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
        const int index = m_legend->indexOfTopLevelItem(item);
        if (column == 0 && index >= 0 && index < m_series.size()) {
            m_series[index].visible = item->checkState(0) == Qt::Checked;
            m_plot->update();
        }
    });

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(m_plot);
    splitter->addWidget(m_legend);
    splitter->setStretchFactor(0, 1);
    splitter->setSizes({520, 230});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addLayout(bar);
    layout->addWidget(splitter);

    connect(m_tester, &Tester::frameSeen, this, &GraphWindow::onFrame);
    connect(m_tester, &Tester::runningChanged, this, [this](bool running) {
        if (running) {
            for (PlotSeries &series : m_series)
                series.points.clear(); // a new measurement starts at 0 s
        }
    });
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        if (!m_pause->isChecked() && m_tester->isRunning()) {
            m_plot->endTime = qMax(m_plot->span, m_tester->time());
            m_plot->database = m_database;
            m_plot->update();
            refreshLegendValues();
        }
    });
    timer->start(50);
}

void GraphWindow::setDatabase(const ff::Database *database)
{
    m_database = database;
    m_plot->database = database;
    m_series.clear();
    rebuildLegend();
    m_plot->update();
}

bool GraphWindow::addSignal(int messageId, int signalId)
{
    if (!m_database || m_series.size() >= MaxSignals)
        return false;
    for (const PlotSeries &series : m_series) {
        if (series.messageId == messageId && series.signalId == signalId)
            return false;
    }
    const ff::Message *message = m_database->findMessage(messageId);
    const ff::Signal *signal = m_database->findSignal(signalId);
    if (!message || !signal)
        return false;
    PlotSeries series;
    series.messageId = messageId;
    series.signalId = signalId;
    series.frameId = message->frameId;
    for (const ff::SignalMapping &mapping : message->signalMappings) {
        if (mapping.signalId == signalId)
            series.startBit = mapping.startBit;
    }
    series.signal = *signal;
    series.label = message->name + '.' + signal->name;
    // The first colour no other series has.
    for (const QColor &color : Colors) {
        bool used = false;
        for (const PlotSeries &other : m_series)
            used = used || other.color == color;
        if (!used) {
            series.color = color;
            break;
        }
    }
    m_series.append(series);
    rebuildLegend();
    m_plot->update();
    return true;
}

void GraphWindow::removeSignal(int index)
{
    if (index < 0 || index >= m_series.size())
        return;
    m_series.removeAt(index);
    rebuildLegend();
    m_plot->update();
}

void GraphWindow::onFrame(double time, quint32 id, const QByteArray &payload, bool)
{
    for (PlotSeries &series : m_series) {
        if (series.frameId != id)
            continue;
        series.points.append(QPointF(time, decodeSignal(series.signal, series.startBit, payload)));
        // Forget what is too old, in chunks so it is not done for every message.
        if (series.points.first().x() < time - HistoryLength - 10) {
            auto keep = std::lower_bound(series.points.begin(), series.points.end(), time - HistoryLength,
                                         [](const QPointF &point, double t) { return point.x() < t; });
            series.points.erase(series.points.begin(), keep);
        }
    }
}

void GraphWindow::fillSignalMenu()
{
    QMenu *menu = m_signalsButton->menu();
    menu->clear();
    if (!m_database || m_database->allMessages().isEmpty()) {
        menu->addAction("Open a database first")->setEnabled(false);
        return;
    }
    for (const ff::Message &message : m_database->allMessages()) {
        QMenu *messageMenu = menu->addMenu(message.name + "  (" + usc::idToText(message.frameId) + ')');
        for (const ff::SignalMapping &mapping : message.signalMappings) {
            const ff::Signal *signal = m_database->findSignal(mapping.signalId);
            if (!signal)
                continue;
            int plotted = -1;
            for (int i = 0; i < m_series.size(); ++i) {
                if (m_series[i].messageId == message.id && m_series[i].signalId == signal->id)
                    plotted = i;
            }
            QAction *action = messageMenu->addAction(signal->name + (signal->unit.isEmpty() ? "" : " [" + signal->unit + ']'));
            action->setCheckable(true);
            action->setChecked(plotted >= 0);
            action->setEnabled(plotted >= 0 || m_series.size() < MaxSignals);
            const int messageId = message.id;
            const int signalId = signal->id;
            connect(action, &QAction::triggered, this, [this, messageId, signalId](bool checked) {
                if (checked) {
                    addSignal(messageId, signalId);
                    return;
                }
                for (int i = 0; i < m_series.size(); ++i) {
                    if (m_series[i].messageId == messageId && m_series[i].signalId == signalId)
                        removeSignal(i);
                }
            });
        }
    }
    menu->addSeparator();
    menu->addAction(QString("%1 of %2 signals plotted").arg(m_series.size()).arg(MaxSignals))->setEnabled(false);
}

void GraphWindow::rebuildLegend()
{
    QSignalBlocker blocker(m_legend);
    m_legend->clear();
    for (int i = 0; i < m_series.size(); ++i) {
        const PlotSeries &series = m_series[i];
        auto *item = new QTreeWidgetItem(m_legend, {series.signal.name, QString()});
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, series.visible ? Qt::Checked : Qt::Unchecked);
        item->setForeground(0, series.color);
        item->setToolTip(0, series.label);
        QPixmap swatch(12, 12);
        swatch.fill(series.color);
        item->setIcon(0, QIcon(swatch));
        auto *remove = new QToolButton;
        remove->setText("×");
        remove->setAutoRaise(true);
        remove->setToolTip("Remove " + series.signal.name);
        const int messageId = series.messageId;
        const int signalId = series.signalId;
        connect(remove, &QToolButton::clicked, this, [this, messageId, signalId] {
            for (int j = 0; j < m_series.size(); ++j) {
                if (m_series[j].messageId == messageId && m_series[j].signalId == signalId) {
                    removeSignal(j);
                    return;
                }
            }
        });
        m_legend->setItemWidget(item, 2, remove);
    }
    refreshLegendValues();
}

void GraphWindow::refreshLegendValues()
{
    QSignalBlocker blocker(m_legend);
    for (int i = 0; i < m_series.size() && i < m_legend->topLevelItemCount(); ++i) {
        const PlotSeries &series = m_series[i];
        m_legend->topLevelItem(i)->setText(
            1, series.points.isEmpty() ? QString("—") : valueText(series, series.points.last().y(), m_database));
    }
}
