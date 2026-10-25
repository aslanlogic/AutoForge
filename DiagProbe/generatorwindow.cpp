#include "generatorwindow.h"
#include "decoding.h"
#include "tester.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace {

enum ListColumn { MessageColumn, IdColumn, ModeColumn, CycleColumn, DataColumn, ActionColumn };

QTableWidgetItem *readOnlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

} // namespace

GeneratorWindow::GeneratorWindow(Tester *tester, QWidget *parent)
    : QWidget(parent)
    , m_tester(tester)
{
    m_addCombo = new QComboBox;
    m_addCombo->setMinimumContentsLength(18);
    auto *addButton = new QPushButton("Add message");
    connect(addButton, &QPushButton::clicked, this, [this] {
        if (m_addCombo->currentIndex() >= 0)
            addMessage(m_addCombo->currentData().toInt());
    });
    auto *addRow = new QHBoxLayout;
    addRow->addWidget(new QLabel("Transmit list"));
    addRow->addStretch();
    addRow->addWidget(m_addCombo);
    addRow->addWidget(addButton);

    m_list = new QTableWidget(0, 6);
    m_list->setHorizontalHeaderLabels({"Message", "ID", "Mode", "Cycle (ms)", "Data", ""});
    m_list->verticalHeader()->hide();
    m_list->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->horizontalHeader()->setSectionResizeMode(DataColumn, QHeaderView::Stretch);
    m_list->setColumnWidth(MessageColumn, 140);
    m_list->setColumnWidth(IdColumn, 70);
    m_list->setColumnWidth(ModeColumn, 85);
    m_list->setColumnWidth(CycleColumn, 100);
    m_list->setColumnWidth(ActionColumn, 70);
    connect(m_list, &QTableWidget::itemSelectionChanged, this, &GeneratorWindow::showSignals);

    m_signalsLabel = new QLabel("Signals");
    m_signals = new QTableWidget(0, 4);
    m_signals->setHorizontalHeaderLabels({"Signal", "Value", "Unit", "Raw"});
    m_signals->verticalHeader()->hide();
    m_signals->setSelectionMode(QAbstractItemView::NoSelection);
    m_signals->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_signals->horizontalHeader()->setStretchLastSection(true);
    m_signals->setColumnWidth(0, 130);
    m_signals->setColumnWidth(1, 150);

    m_rawId = new QLineEdit("0x400");
    m_rawId->setMaximumWidth(90);
    m_rawData = new QLineEdit("01 02 03 04");
    auto *rawSend = new QPushButton("Send");
    connect(rawSend, &QPushButton::clicked, this, &GeneratorWindow::sendRawFrame);
    connect(m_rawData, &QLineEdit::returnPressed, this, &GeneratorWindow::sendRawFrame);
    m_rawStatus = new QLabel;
    auto *rawRow = new QHBoxLayout;
    rawRow->addWidget(new QLabel("Raw frame  ID:"));
    rawRow->addWidget(m_rawId);
    rawRow->addWidget(new QLabel("Data:"));
    rawRow->addWidget(m_rawData, 1);
    rawRow->addWidget(rawSend);
    rawRow->addWidget(m_rawStatus);

    auto *top = new QWidget;
    auto *topLayout = new QVBoxLayout(top);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->addLayout(addRow);
    topLayout->addWidget(m_list);
    auto *bottom = new QWidget;
    auto *bottomLayout = new QVBoxLayout(bottom);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->addWidget(m_signalsLabel);
    bottomLayout->addWidget(m_signals);
    auto *splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(top);
    splitter->addWidget(bottom);
    splitter->setChildrenCollapsible(false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(splitter);
    layout->addLayout(rawRow);

    // Cyclic sending ends with the measurement.
    connect(m_tester, &Tester::runningChanged, this, [this](bool running) {
        if (!running) {
            for (int i = 0; i < m_entries.size(); ++i)
                setCyclic(i, false);
        }
    });
}

void GeneratorWindow::setDatabase(const ff::Database *database)
{
    for (Entry &entry : m_entries)
        delete entry.timer;
    m_entries.clear();
    m_database = database;
    m_addCombo->clear();
    if (database) {
        for (const ff::Message &message : database->allMessages())
            m_addCombo->addItem(message.name + "  (" + usc::idToText(message.frameId) + ')', message.id);
        for (const ff::Message &message : database->allMessages()) {
            const ff::Node *transmitter = database->findNode(message.transmitterId);
            if (transmitter && transmitter->name.compare("Tester", Qt::CaseInsensitive) == 0)
                addMessage(message.id);
        }
    }
    rebuildList();
}

bool GeneratorWindow::addMessage(int messageId)
{
    const ff::Message *message = m_database ? m_database->findMessage(messageId) : nullptr;
    if (!message)
        return false;
    Entry entry;
    entry.messageId = messageId;
    for (const ff::SignalMapping &mapping : message->signalMappings) {
        if (const ff::Signal *signal = m_database->findSignal(mapping.signalId)) {
            double value = signal->initialValue;
            if (signal->maximum > signal->minimum)
                value = qBound(signal->minimum, value, signal->maximum);
            entry.values.insert(signal->id, value);
        }
    }
    entry.timer = new QTimer(this);
    entry.timer->setInterval(isCyclic(message->txMethod) && message->cycleTime > 0 ? message->cycleTime : 100);
    encode(entry);
    m_entries.append(entry);
    const int index = m_entries.size() - 1;
    connect(entry.timer, &QTimer::timeout, this, [this, timer = entry.timer] {
        for (int i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].timer == timer)
                sendEntry(i);
        }
    });
    rebuildList();
    m_list->selectRow(index);
    return true;
}

void GeneratorWindow::encode(Entry &entry)
{
    const ff::Message *message = m_database->findMessage(entry.messageId);
    entry.payload = QByteArray(message ? message->length : 0, '\0');
    if (!message)
        return;
    for (const ff::SignalMapping &mapping : message->signalMappings) {
        if (const ff::Signal *signal = m_database->findSignal(mapping.signalId))
            encodeSignal(*signal, mapping.startBit, entry.values.value(signal->id), &entry.payload);
    }
}

void GeneratorWindow::rebuildList()
{
    const int selected = m_list->currentRow();
    m_list->setRowCount(m_entries.size());
    for (int index = 0; index < m_entries.size(); ++index) {
        const Entry &entry = m_entries[index];
        const ff::Message *message = m_database->findMessage(entry.messageId);
        m_list->setItem(index, MessageColumn, readOnlyItem(message ? message->name : QString()));
        m_list->setItem(index, IdColumn, readOnlyItem(message ? usc::idToText(message->frameId) : QString()));

        auto *mode = new QComboBox;
        mode->addItems({"Once", "Cyclic"});
        mode->setCurrentIndex(entry.timer->isActive() ? 1 : 0);
        auto *cycle = new QSpinBox;
        cycle->setRange(1, 60000);
        cycle->setValue(entry.timer->interval());
        auto *action = new QPushButton;
        connect(mode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, index] {
            setCyclic(index, false);
            updateRow(index);
        });
        connect(cycle, qOverload<int>(&QSpinBox::valueChanged), this, [this, index](int value) {
            m_entries[index].timer->setInterval(value);
        });
        connect(action, &QPushButton::clicked, this, [this, index] {
            auto *modeBox = qobject_cast<QComboBox *>(m_list->cellWidget(index, ModeColumn));
            if (modeBox && modeBox->currentIndex() == 1)
                setCyclic(index, !m_entries[index].timer->isActive());
            else
                sendEntry(index);
        });
        m_list->setCellWidget(index, ModeColumn, mode);
        m_list->setCellWidget(index, CycleColumn, cycle);
        m_list->setCellWidget(index, ActionColumn, action);
        updateRow(index);
    }
    if (selected >= 0 && selected < m_entries.size())
        m_list->selectRow(selected);
    showSignals();
}

void GeneratorWindow::updateRow(int index)
{
    const Entry &entry = m_entries[index];
    m_list->setItem(index, DataColumn, readOnlyItem(hexBytes(entry.payload)));
    auto *mode = qobject_cast<QComboBox *>(m_list->cellWidget(index, ModeColumn));
    auto *action = qobject_cast<QPushButton *>(m_list->cellWidget(index, ActionColumn));
    if (!mode || !action)
        return;
    if (mode->currentIndex() == 0)
        action->setText("Send");
    else
        action->setText(entry.timer->isActive() ? "Stop" : "Start");
}

void GeneratorWindow::showSignals()
{
    const int index = m_list->currentRow();
    m_signals->setRowCount(0);
    if (index < 0 || index >= m_entries.size()) {
        m_signalsLabel->setText("Signals: select a message in the transmit list");
        return;
    }
    const ff::Message *message = m_database->findMessage(m_entries[index].messageId);
    if (!message)
        return;
    m_signalsLabel->setText("Signals of " + message->name);
    for (const ff::SignalMapping &mapping : message->signalMappings) {
        const ff::Signal *signal = m_database->findSignal(mapping.signalId);
        if (!signal)
            continue;
        const int row = m_signals->rowCount();
        m_signals->insertRow(row);
        m_signals->setItem(row, 0, readOnlyItem(signal->name));
        m_signals->setItem(row, 2, readOnlyItem(signal->unit));
        const double value = m_entries[index].values.value(signal->id);
        const int signalId = signal->id;
        const double factor = signal->factor != 0 ? signal->factor : 1;
        auto rawText = [signal, factor](double physical) {
            return QString::number(std::llround((physical - signal->offset) / factor));
        };
        m_signals->setItem(row, 3, readOnlyItem(rawText(value)));

        const ff::ValueTable *table = m_database->findValueTable(signal->valueTableId);
        if (table && !table->entries.isEmpty()) {
            auto *combo = new QComboBox;
            for (const ff::ValueTable::Entry &entry : table->entries)
                combo->addItem(QString("%1 (%2)").arg(entry.description).arg(entry.value),
                               double(entry.value) * signal->factor + signal->offset);
            combo->setCurrentIndex(qMax(0, combo->findData(value)));
            connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, index, signalId, combo] {
                setSignalValue(index, signalId, combo->currentData().toDouble());
            });
            m_signals->setCellWidget(row, 1, combo);
        } else {
            auto *spin = new QDoubleSpinBox;
            const bool ranged = signal->maximum > signal->minimum;
            spin->setRange(ranged ? signal->minimum : -1e12, ranged ? signal->maximum : 1e12);
            const int decimals = factor < 1 ? qBound(0, int(std::ceil(-std::log10(factor))), 6) : 0;
            spin->setDecimals(decimals);
            spin->setSingleStep(factor);
            spin->setValue(value);
            connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, index, signalId](double v) {
                setSignalValue(index, signalId, v);
            });
            m_signals->setCellWidget(row, 1, spin);
        }
    }
}

bool GeneratorWindow::setSignalValue(int index, int signalId, double physical)
{
    if (index < 0 || index >= m_entries.size() || !m_entries[index].values.contains(signalId))
        return false;
    Entry &entry = m_entries[index];
    entry.values.insert(signalId, physical);
    encode(entry);
    updateRow(index);
    // The raw column of the signal editor, if it shows this entry.
    if (m_list->currentRow() == index) {
        const ff::Message *message = m_database->findMessage(entry.messageId);
        int row = 0;
        for (const ff::SignalMapping &mapping : message->signalMappings) {
            const ff::Signal *signal = m_database->findSignal(mapping.signalId);
            if (!signal)
                continue;
            if (signal->id == signalId && m_signals->item(row, 3)) {
                const double factor = signal->factor != 0 ? signal->factor : 1;
                m_signals->item(row, 3)->setText(QString::number(std::llround((physical - signal->offset) / factor)));
            }
            ++row;
        }
    }
    return true;
}

QByteArray GeneratorWindow::payloadOf(int index) const
{
    return index >= 0 && index < m_entries.size() ? m_entries[index].payload : QByteArray();
}

void GeneratorWindow::sendEntry(int index)
{
    if (index < 0 || index >= m_entries.size())
        return;
    if (const ff::Message *message = m_database->findMessage(m_entries[index].messageId))
        m_tester->sendFrame(message->frameId, m_entries[index].payload);
}

void GeneratorWindow::setCyclic(int index, bool running)
{
    if (index < 0 || index >= m_entries.size())
        return;
    QTimer *timer = m_entries[index].timer;
    if (running && m_tester->isRunning()) {
        timer->start();
        sendEntry(index);
    } else {
        timer->stop();
    }
    auto *mode = qobject_cast<QComboBox *>(m_list->cellWidget(index, ModeColumn));
    if (running && mode && mode->currentIndex() != 1) {
        QSignalBlocker blocker(mode);
        mode->setCurrentIndex(1);
    }
    updateRow(index);
}

void GeneratorWindow::sendRawFrame()
{
    bool ok = false;
    QString idText = m_rawId->text().trimmed();
    const quint32 id = idText.startsWith("0x", Qt::CaseInsensitive) ? idText.mid(2).toUInt(&ok, 16)
                                                                     : idText.toUInt(&ok, 16);
    QByteArray data;
    if (!ok) {
        m_rawStatus->setText("ID is not hex");
        return;
    }
    if (!parseHex(m_rawData->text(), &data)) {
        m_rawStatus->setText("Data is not hex");
        return;
    }
    if (!m_tester->isRunning()) {
        m_rawStatus->setText("Not measuring");
        return;
    }
    m_rawStatus->clear();
    m_tester->sendFrame(id, data);
}
