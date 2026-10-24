#include "tracewindow.h"
#include "decoding.h"
#include "tester.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

// A signal row has no time of its own, so its Time cell holds the Graph button.
enum Column { TimeColumn, IdColumn, NameColumn, DirColumn, DlcColumn, DataColumn, CycleColumn, CountColumn,
              ColumnCount };

const int IdRole = Qt::UserRole;
const int MaxChronologicalRows = 3000;

QString idText(quint32 id)
{
    return usc::idToText(id);
}

} // namespace

TraceWindow::TraceWindow(Tester *tester, QWidget *parent)
    : QWidget(parent)
    , m_tester(tester)
{
    QFont mono("monospace");
    mono.setStyleHint(QFont::TypeWriter);

    m_byMessage = new QTreeWidget;
    m_byMessage->setColumnCount(ColumnCount);
    m_byMessage->setHeaderLabels({"Time", "ID", "Name", "Dir", "DLC", "Data", "Cycle (ms)", "Count"});
    m_byMessage->setFont(mono);
    m_byMessage->setUniformRowHeights(true);
    m_byMessage->setColumnWidth(TimeColumn, 110);
    m_byMessage->setColumnWidth(IdColumn, 70);
    m_byMessage->setColumnWidth(NameColumn, 150);
    m_byMessage->setColumnWidth(DirColumn, 36);
    m_byMessage->setColumnWidth(DlcColumn, 40);
    m_byMessage->setColumnWidth(DataColumn, 300);
    m_byMessage->setColumnWidth(CycleColumn, 80);
    m_byMessage->setColumnWidth(CountColumn, 60);

    m_chronological = new QTableWidget(0, 7);
    m_chronological->setHorizontalHeaderLabels({"Time", "ID", "Name", "Dir", "DLC", "Data", "Interpretation"});
    m_chronological->setFont(mono);
    m_chronological->verticalHeader()->hide();
    m_chronological->verticalHeader()->setDefaultSectionSize(m_chronological->fontMetrics().height() + 4);
    m_chronological->horizontalHeader()->setStretchLastSection(true);
    m_chronological->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_chronological->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_chronological->setColumnWidth(0, 90);
    m_chronological->setColumnWidth(1, 70);
    m_chronological->setColumnWidth(2, 120);
    m_chronological->setColumnWidth(3, 36);
    m_chronological->setColumnWidth(4, 40);
    m_chronological->setColumnWidth(5, 260);

    auto *tabs = new QTabWidget;
    tabs->addTab(m_byMessage, "By message");
    tabs->addTab(m_chronological, "Chronological");

    m_pause = new QCheckBox("Pause");
    auto *clearButton = new QPushButton("Clear");
    connect(clearButton, &QPushButton::clicked, this, &TraceWindow::clear);
    auto *bar = new QHBoxLayout;
    bar->addWidget(m_pause);
    bar->addWidget(clearButton);
    bar->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addLayout(bar);
    layout->addWidget(tabs);

    connect(m_tester, &Tester::frameSeen, this, &TraceWindow::onFrame);
    auto *timer = new QTimer(this); // the display is refreshed in batches, not per message
    connect(timer, &QTimer::timeout, this, &TraceWindow::flush);
    timer->start(100);
}

void TraceWindow::setDatabases(const usc::Database *description, const ff::Database *database)
{
    m_description = description;
    m_database = database;
    clear();
}

void TraceWindow::clear()
{
    m_rows.clear();
    m_pending.clear();
    m_byMessage->clear();
    m_chronological->setRowCount(0);
}

bool TraceWindow::isRequestId(quint32 id) const
{
    return m_description && id == m_description->network().requestId;
}

bool TraceWindow::isResponseId(quint32 id) const
{
    return m_description && id == m_description->network().responseId;
}

QString TraceWindow::nameOf(quint32 id) const
{
    if (isRequestId(id))
        return "DiagRequest";
    if (isResponseId(id))
        return "DiagResponse";
    const ff::Message *message = m_database ? findMessageByFrameId(*m_database, id) : nullptr;
    return message ? message->name : QString("—");
}

QString TraceWindow::interpretation(quint32 id, const QByteArray &data) const
{
    if (isRequestId(id) || isResponseId(id))
        return describeUds(m_description, data);
    const ff::Message *message = m_database ? findMessageByFrameId(*m_database, id) : nullptr;
    if (!message)
        return QString();
    QStringList parts;
    for (const DecodedValue &value : decodeMessage(*m_database, *message, data)) {
        parts << value.name + '=' + (value.valueText.isEmpty() ? value.physicalText + (value.unit.isEmpty() ? "" : ' ' + value.unit)
                                                              : value.valueText);
    }
    return parts.join("  ");
}

void TraceWindow::onFrame(double time, quint32 id, const QByteArray &data, bool transmitted)
{
    MessageRow &row = rowFor(id, transmitted);
    if (row.lastTime >= 0)
        row.cycle = time - row.lastTime;
    row.lastTime = time;
    ++row.count;
    row.data = data;
    row.dirty = true;
    if (!m_pause->isChecked())
        m_pending.append(Frame{time, id, data, transmitted});
}

TraceWindow::MessageRow &TraceWindow::rowFor(quint32 id, bool transmitted)
{
    const quint64 key = (quint64(transmitted) << 32) | id;
    auto it = m_rows.find(key);
    if (it != m_rows.end())
        return *it;

    MessageRow row;
    row.id = id;
    row.transmitted = transmitted;
    row.item = new QTreeWidgetItem;
    row.item->setData(0, IdRole, QVariant::fromValue(key));
    // Keep the rows sorted by ID, received before sent.
    int index = 0;
    while (index < m_byMessage->topLevelItemCount()
           && m_byMessage->topLevelItem(index)->data(0, IdRole).toULongLong() < key)
        ++index;
    m_byMessage->insertTopLevelItem(index, row.item);
    row.item->setText(IdColumn, idText(id));
    row.item->setText(NameColumn, nameOf(id));
    row.item->setText(DirColumn, transmitted ? "Tx" : "Rx");

    // The children: the signals of a known message, or the explained UDS message.
    const ff::Message *message = m_database ? findMessageByFrameId(*m_database, id) : nullptr;
    if (isRequestId(id) || isResponseId(id)) {
        auto *child = new QTreeWidgetItem(row.item);
        child->setText(NameColumn, "UDS");
    } else if (message) {
        for (const ff::SignalMapping &mapping : message->signalMappings) {
            const ff::Signal *signal = m_database->findSignal(mapping.signalId);
            if (!signal)
                continue;
            auto *child = new QTreeWidgetItem(row.item);
            child->setText(NameColumn, signal->name);
            auto *button = new QToolButton;
            button->setText("Graph");
            button->setAutoRaise(true);
            button->setToolTip("Show " + signal->name + " in the Graphics window");
            const int messageId = message->id;
            const int signalId = signal->id;
            connect(button, &QToolButton::clicked, this, [this, messageId, signalId] {
                emit graphRequested(messageId, signalId);
            });
            m_byMessage->setItemWidget(child, TimeColumn, button);
        }
    }
    return *m_rows.insert(key, row);
}

void TraceWindow::updateRow(MessageRow &row)
{
    row.dirty = false;
    row.item->setText(TimeColumn, QString::number(row.lastTime, 'f', 3));
    row.item->setText(DlcColumn, QString::number(row.data.size()));
    row.item->setText(DataColumn, hexBytes(row.data));
    row.item->setText(CycleColumn, row.cycle > 0 ? QString::number(row.cycle * 1000.0, 'f', 0) : QString());
    row.item->setText(CountColumn, QString::number(row.count));

    if (isRequestId(row.id) || isResponseId(row.id)) {
        if (row.item->childCount() > 0) {
            row.item->child(0)->setText(DataColumn, describeUds(m_description, row.data));
            row.item->child(0)->setForeground(DataColumn, isNegativeResponse(row.data) ? QColor(200, 30, 30) : QColor());
        }
        return;
    }
    const ff::Message *message = m_database ? findMessageByFrameId(*m_database, row.id) : nullptr;
    if (!message || !row.item->isExpanded())
        return;
    const QVector<DecodedValue> values = decodeMessage(*m_database, *message, row.data);
    for (int i = 0; i < values.size() && i < row.item->childCount(); ++i) {
        const DecodedValue &value = values[i];
        QString text = value.physicalText + (value.unit.isEmpty() ? QString() : ' ' + value.unit);
        if (!value.valueText.isEmpty())
            text += "  \"" + value.valueText + '"';
        text += "   (raw " + value.raw + ')';
        row.item->child(i)->setText(DataColumn, text);
    }
}

void TraceWindow::flush()
{
    if (m_pause->isChecked())
        return;
    for (MessageRow &row : m_rows) {
        if (row.dirty || row.item->isExpanded())
            updateRow(row);
    }

    if (m_pending.isEmpty())
        return;
    if (m_pending.size() > MaxChronologicalRows)
        m_pending.remove(0, m_pending.size() - MaxChronologicalRows);
    const int overflow = m_chronological->rowCount() + m_pending.size() - MaxChronologicalRows;
    for (int i = 0; i < overflow; ++i)
        m_chronological->removeRow(0);
    m_chronological->setUpdatesEnabled(false);
    for (const Frame &frame : qAsConst(m_pending)) {
        const int row = m_chronological->rowCount();
        m_chronological->insertRow(row);
        const QStringList texts = {QString::number(frame.time, 'f', 3), idText(frame.id), nameOf(frame.id),
                                   frame.transmitted ? "Tx" : "Rx", QString::number(frame.data.size()),
                                   hexBytes(frame.data), interpretation(frame.id, frame.data)};
        for (int column = 0; column < texts.size(); ++column)
            m_chronological->setItem(row, column, new QTableWidgetItem(texts[column]));
        if (isResponseId(frame.id) && isNegativeResponse(frame.data)) {
            for (int column = 0; column < texts.size(); ++column)
                m_chronological->item(row, column)->setForeground(QColor(200, 30, 30));
        }
    }
    m_pending.clear();
    m_chronological->setUpdatesEnabled(true);
    m_chronological->scrollToBottom();
}
