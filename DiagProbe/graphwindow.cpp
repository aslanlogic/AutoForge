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
#include <QPainter>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>

#include <cmath>

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
class PlotWidget : public QWidget
{
public:
    PlotWidget(const QVector<PlotSeries> *series, QWidget *parent = nullptr)
        : QWidget(parent), m_series(series)
    {
        setMouseTracking(true);
        setMinimumSize(300, 160);
        setAutoFillBackground(true);
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, Qt::white);
        setPalette(palette);
    }

    GraphWindow::AxisMode axisMode = GraphWindow::AxisMode::PerSignal;
    double span = 10.0;   // s
    double endTime = 0.0; // s; the right edge
    const ff::Database *database = nullptr;

protected:
    void mouseMoveEvent(QMouseEvent *event) override
    {
        m_hoverX = event->pos().x();
        update();
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
        QVector<const PlotSeries *> visible;
        for (const PlotSeries &series : *m_series) {
            if (series.visible)
                visible.append(&series);
        }

        const int axisWidth = 52;
        const int axisCount = axisMode == GraphWindow::AxisMode::PerSignal ? qMax(1, int(visible.size())) : 1;
        const QRectF plot(axisWidth * axisCount + 6, 8, width() - axisWidth * axisCount - 26, height() - 30);
        if (plot.width() < 20 || plot.height() < 20)
            return;
        const double startTime = endTime - span;
        auto xOf = [&](double t) { return plot.left() + (t - startTime) / span * plot.width(); };

        // Time grid and labels.
        painter.setPen(QColor(225, 225, 225));
        const double step = span / 10.0;
        for (double t = std::ceil(startTime / step) * step; t <= endTime + 1e-9; t += step) {
            painter.drawLine(QPointF(xOf(t), plot.top()), QPointF(xOf(t), plot.bottom()));
        }
        painter.setPen(Qt::darkGray);
        for (double t = std::ceil(startTime / (step * 2)) * step * 2; t <= endTime + 1e-9; t += step * 2) {
            if (t >= 0)
                painter.drawText(QRectF(xOf(t) - 30, plot.bottom() + 2, 60, 18), Qt::AlignCenter,
                                 QString::number(t, 'f', 0) + " s");
        }
        painter.drawRect(plot);

        if (visible.isEmpty()) {
            painter.drawText(plot, Qt::AlignCenter, "Add a signal with Signals ▾ or a Trace window's Graph button.");
            return;
        }

        // The y range of each visible series, and the lane it is drawn in.
        QVector<QPair<double, double>> ranges;
        for (const PlotSeries *series : visible)
            ranges.append(rangeOf(*series));
        if (axisMode == GraphWindow::AxisMode::Shared) {
            QPair<double, double> all = ranges.first();
            for (const auto &range : ranges) {
                all.first = qMin(all.first, range.first);
                all.second = qMax(all.second, range.second);
            }
            for (auto &range : ranges)
                range = all;
        }
        auto laneOf = [&](int index) {
            if (axisMode != GraphWindow::AxisMode::Stacked)
                return plot;
            const double laneHeight = plot.height() / visible.size();
            return QRectF(plot.left(), plot.top() + index * laneHeight, plot.width(), laneHeight);
        };
        auto yOf = [&](int index, double value) {
            const QRectF lane = laneOf(index).adjusted(0, 4, 0, -4);
            const auto &range = ranges[index];
            return lane.bottom() - (value - range.first) / (range.second - range.first) * lane.height();
        };

        // Axes.
        for (int i = 0; i < visible.size(); ++i) {
            const QRectF lane = laneOf(i).adjusted(0, 4, 0, -4);
            const double x = axisMode == GraphWindow::AxisMode::PerSignal ? plot.left() - axisWidth * i - 2
                                                                           : plot.left() - 2;
            if (axisMode == GraphWindow::AxisMode::Shared && i > 0)
                break;
            painter.setPen(axisMode == GraphWindow::AxisMode::Shared ? Qt::darkGray : visible[i]->color);
            painter.drawLine(QPointF(x, lane.top()), QPointF(x, lane.bottom()));
            for (int tick = 0; tick <= 4; ++tick) {
                const double value = ranges[i].first + (ranges[i].second - ranges[i].first) * tick / 4.0;
                const double y = yOf(i, value);
                if (axisMode == GraphWindow::AxisMode::Stacked && (tick == 1 || tick == 3))
                    continue;
                painter.drawLine(QPointF(x - 3, y), QPointF(x, y));
                // In lanes, the top and bottom labels move inwards so neighbouring lanes do not overlap.
                double shift = 0;
                if (axisMode == GraphWindow::AxisMode::Stacked)
                    shift = tick == 4 ? 6 : tick == 0 ? -6 : 0;
                painter.drawText(QRectF(x - axisWidth, y - 8 + shift, axisWidth - 5, 16),
                                 Qt::AlignRight | Qt::AlignVCenter, formatNumber(std::round(value * 100) / 100));
            }
            if (axisMode == GraphWindow::AxisMode::Stacked && i > 0) {
                painter.setPen(QColor(210, 210, 210));
                painter.drawLine(QPointF(plot.left(), laneOf(i).top()), QPointF(plot.right(), laneOf(i).top()));
            }
        }

        // The curves.
        painter.save();
        painter.setClipRect(plot);
        for (int i = 0; i < visible.size(); ++i) {
            const QVector<QPointF> &points = visible[i]->points;
            QPolygonF line;
            auto first = std::lower_bound(points.begin(), points.end(), startTime,
                                          [](const QPointF &point, double t) { return point.x() < t; });
            if (first != points.begin())
                --first; // one more to the left, so the line starts at the edge
            for (auto it = first; it != points.end(); ++it)
                line << QPointF(xOf(it->x()), yOf(i, it->y()));
            painter.setPen(QPen(visible[i]->color, 1.6));
            painter.drawPolyline(line);
        }
        painter.restore();

        // The cursor: a line and every value at that time.
        if (m_hoverX >= plot.left() && m_hoverX <= plot.right()) {
            const double t = startTime + (m_hoverX - plot.left()) / plot.width() * span;
            painter.setPen(QPen(Qt::black, 1, Qt::DashLine));
            painter.drawLine(QPointF(m_hoverX, plot.top()), QPointF(m_hoverX, plot.bottom()));
            QStringList lines = {QString("t = %1 s").arg(t, 0, 'f', 3)};
            QVector<QColor> colors = {Qt::black};
            for (int i = 0; i < visible.size(); ++i) {
                double value;
                if (valueAt(*visible[i], t, &value)) {
                    lines << visible[i]->signal.name + ": " + valueText(*visible[i], value, database);
                    colors << visible[i]->color;
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
    const QVector<PlotSeries> *m_series;
    double m_hoverX = -1;
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
    connect(m_span, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        m_plot->span = m_span->currentData().toDouble();
        m_plot->update();
    });

    m_pause = new QCheckBox("Pause");

    auto *bar = new QHBoxLayout;
    bar->addWidget(m_signalsButton);
    bar->addWidget(new QLabel("Axes:"));
    bar->addWidget(m_axisMode);
    bar->addWidget(new QLabel("Span:"));
    bar->addWidget(m_span);
    bar->addWidget(m_pause);
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
