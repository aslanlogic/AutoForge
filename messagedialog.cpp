#include "messagedialog.h"
#include "messagelayoutwidget.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

// Accepts hex with a 0x prefix ("0x1A") or plain decimal ("26").
bool textToFrameId(const QString &text, quint32 *frameId)
{
    const QString trimmed = text.trimmed();
    bool ok = false;
    if (trimmed.startsWith("0x", Qt::CaseInsensitive))
        *frameId = trimmed.mid(2).toUInt(&ok, 16);
    else
        *frameId = trimmed.toUInt(&ok, 10);
    return ok;
}

} // namespace

MessageDialog::MessageDialog(const Message &message, const QStringList &existingNames,
                             const QVector<Signal> &allSignals, QWidget *parent)
    : QDialog(parent)
    , m_message(message)
    , m_existingNames(existingNames)
    , m_mappings(message.signalMappings)
{
    for (const Signal &signal : allSignals)
        m_signalsById.insert(signal.id, signal);

    setWindowTitle(QString("Message '%1 (%2)'").arg(message.name, frameIdToText(message.frameId)));

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createDefinitionTab(), "Definition");
    m_tabs->addTab(createSignalsTab(), "Signals");
    m_tabs->addTab(createLayoutTab(), "Layout");
    m_tabs->addTab(createCommentTab(), "Comment");

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_tabs);
    layout->addWidget(buttons);

    // Combo box item order matches the enum order in Message.
    // Type goes before length: changing the type changes the allowed length range.
    m_nameEdit->setText(message.name);
    m_typeCombo->setCurrentIndex(static_cast<int>(message.type));
    onTypeChanged();
    m_frameIdEdit->setText(frameIdToText(message.frameId));
    m_lengthSpin->setValue(message.length);
    m_txMethodCombo->setCurrentIndex(static_cast<int>(message.txMethod));
    onTxMethodChanged();
    m_cycleTimeSpin->setValue(message.cycleTime);
    m_commentEdit->setPlainText(message.comment);
    refreshSignals();

    // The layout grid has one row per byte: redraw it when the length changes.
    connect(m_lengthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MessageDialog::refreshSignals);

    resize(640, 520);
}

Message MessageDialog::message() const
{
    // Start from the message we were given, so fields the dialog does not show (like id) are kept.
    Message message = m_message;
    message.name = m_nameEdit->text().trimmed();
    message.type = currentType();
    textToFrameId(m_frameIdEdit->text(), &message.frameId);
    message.length = m_lengthSpin->value();
    message.txMethod = currentTxMethod();
    message.cycleTime = m_cycleTimeSpin->value();
    message.signalMappings = m_mappings;
    message.comment = m_commentEdit->toPlainText();
    return message;
}

void MessageDialog::accept()
{
    const QString name = m_nameEdit->text().trimmed();
    const Message::Type type = currentType();
    quint32 frameId = 0;

    QString error;
    QWidget *badField = nullptr;
    if (name.isEmpty()) {
        error = "Please enter a name for the message.";
        badField = m_nameEdit;
    } else if (m_existingNames.contains(name)) {
        error = QString("A message named '%1' already exists.").arg(name);
        badField = m_nameEdit;
    } else if (!textToFrameId(m_frameIdEdit->text(), &frameId) || frameId > maxFrameId(type)) {
        error = QString("The ID must be a number from 0x0 to %1 for %2.\n"
                        "Write hex with 0x in front (0x1A) or plain decimal (26).")
                    .arg(frameIdToText(maxFrameId(type)), toString(type));
        badField = m_frameIdEdit;
    } else if (!isValidLength(type, m_lengthSpin->value())) {
        error = "CAN FD only allows 0-8, 12, 16, 20, 24, 32, 48 or 64 bytes.";
        badField = m_lengthSpin;
    }

    if (!error.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), error);
        m_tabs->setCurrentIndex(0);
        badField->setFocus();
        if (auto *edit = qobject_cast<QLineEdit *>(badField))
            edit->selectAll();
        return;
    }

    // E.g. the length was made smaller after the signals were placed.
    const QString layoutError = checkSignalLayout(m_lengthSpin->value(), m_mappings, m_signalsById);
    if (!layoutError.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), layoutError);
        m_tabs->setCurrentWidget(m_layoutTab);
        return;
    }

    QDialog::accept();
}

QWidget *MessageDialog::createDefinitionTab()
{
    auto *tab = new QWidget;

    m_nameEdit = new QLineEdit;

    m_typeCombo = new QComboBox;
    for (auto type : {Message::Type::CanStandard, Message::Type::CanExtended,
                      Message::Type::CanFdStandard, Message::Type::CanFdExtended,
                      Message::Type::Uart, Message::Type::Udp})
        m_typeCombo->addItem(toString(type));
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MessageDialog::onTypeChanged);

    m_frameIdEdit = new QLineEdit;

    m_lengthLabel = new QLabel;
    m_lengthSpin = new QSpinBox;

    // There are no network nodes yet to choose from.
    m_transmitterCombo = new QComboBox;
    m_transmitterCombo->addItem("-- No Transmitter --");
    m_transmitterCombo->setEnabled(false);

    m_txMethodCombo = new QComboBox;
    for (auto txMethod : {Message::TxMethod::Cyclic, Message::TxMethod::OnEvent,
                          Message::TxMethod::CyclicAndOnEvent})
        m_txMethodCombo->addItem(toString(txMethod));
    connect(m_txMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MessageDialog::onTxMethodChanged);

    m_cycleTimeSpin = new QSpinBox;
    m_cycleTimeSpin->setRange(1, 3600000);
    m_cycleTimeSpin->setSuffix(" ms");

    auto *grid = new QGridLayout;
    grid->addWidget(new QLabel("Name:"), 0, 0);
    grid->addWidget(m_nameEdit, 0, 1, 1, 3);
    grid->addWidget(new QLabel("Type:"), 1, 0);
    grid->addWidget(m_typeCombo, 1, 1, 1, 3);
    grid->addWidget(new QLabel("ID:"), 2, 0);
    grid->addWidget(m_frameIdEdit, 2, 1);
    grid->addWidget(m_lengthLabel, 2, 2);
    grid->addWidget(m_lengthSpin, 2, 3);
    grid->addWidget(new QLabel("Transmitter:"), 3, 0);
    grid->addWidget(m_transmitterCombo, 3, 1, 1, 3);
    grid->addWidget(new QLabel("Tx Method:"), 4, 0);
    grid->addWidget(m_txMethodCombo, 4, 1, 1, 3);
    grid->addWidget(new QLabel("Cycle Time:"), 5, 0);
    grid->addWidget(m_cycleTimeSpin, 5, 1, 1, 3);
    // ID and DLC share one row: give them equal width.
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);

    auto *layout = new QVBoxLayout(tab);
    layout->addLayout(grid);
    layout->addStretch();

    return tab;
}

QWidget *MessageDialog::createSignalsTab()
{
    auto *tab = new QWidget;

    m_signalsTable = new QTableWidget(0, 4);
    m_signalsTable->setHorizontalHeaderLabels({"Name", "Start Bit", "Length [Bit]", "Byte Order"});
    m_signalsTable->horizontalHeader()->setStretchLastSection(true);
    m_signalsTable->verticalHeader()->setVisible(false);
    m_signalsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_signalsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_signalsTable->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *addButton = new QPushButton("Add...");
    auto *removeButton = new QPushButton("Remove");
    connect(addButton, &QPushButton::clicked, this, &MessageDialog::addSignal);
    connect(removeButton, &QPushButton::clicked, this, &MessageDialog::removeSignal);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(addButton);
    buttons->addWidget(removeButton);
    buttons->addStretch();

    auto *layout = new QVBoxLayout(tab);
    layout->addWidget(m_signalsTable);
    layout->addLayout(buttons);

    return tab;
}

QWidget *MessageDialog::createLayoutTab()
{
    m_layoutWidget = new MessageLayoutWidget;
    connect(m_layoutWidget, &MessageLayoutWidget::signalMoved, this, &MessageDialog::moveSignal);

    // Long payloads (CAN FD, UART, UDP) do not fit on screen: scroll.
    auto *scrollArea = new QScrollArea;
    scrollArea->setWidget(m_layoutWidget);
    scrollArea->setWidgetResizable(false);
    m_layoutTab = scrollArea;
    return scrollArea;
}

QWidget *MessageDialog::createCommentTab()
{
    auto *tab = new QWidget;

    m_commentEdit = new QPlainTextEdit;

    auto *layout = new QVBoxLayout(tab);
    layout->addWidget(m_commentEdit);

    return tab;
}

Message::Type MessageDialog::currentType() const
{
    return static_cast<Message::Type>(m_typeCombo->currentIndex());
}

Message::TxMethod MessageDialog::currentTxMethod() const
{
    return static_cast<Message::TxMethod>(m_txMethodCombo->currentIndex());
}

void MessageDialog::onTypeChanged()
{
    const Message::Type type = currentType();

    // CAN calls the payload length "DLC".
    m_lengthLabel->setText(isCan(type) ? "DLC:" : "Length [Byte]:");
    // setRange() also lowers the current value if it is now too large.
    m_lengthSpin->setRange(0, maxLength(type));
}

void MessageDialog::onTxMethodChanged()
{
    m_cycleTimeSpin->setEnabled(isCyclic(currentTxMethod()));
}

void MessageDialog::refreshSignals()
{
    m_signalsTable->setRowCount(m_mappings.size());
    for (int row = 0; row < m_mappings.size(); ++row) {
        const Signal signal = m_signalsById.value(m_mappings.at(row).signalId);
        const QStringList cells = {signal.name, QString::number(m_mappings.at(row).startBit),
                                   QString::number(signal.length), toString(signal.byteOrder)};
        for (int column = 0; column < cells.size(); ++column) {
            auto *item = new QTableWidgetItem(cells.at(column));
            // Same colour as in the Layout tab, so the two are easy to match.
            item->setBackground(MessageLayoutWidget::colorFor(row));
            item->setForeground(Qt::black);
            m_signalsTable->setItem(row, column, item);
        }
    }

    m_layoutWidget->setLayoutData(m_lengthSpin->value(), m_mappings, m_signalsById);
}

int MessageDialog::findFreeStartBit(const Signal &signal) const
{
    // Try positions from the start of the payload; the first one that fits wins.
    // For Motorola the start bit is the top bit of a byte first (7, 6, ... 0, 15, ...)
    // so that the signal is packed from the top-left of the layout grid.
    QVector<SignalMapping> candidate = m_mappings;
    candidate.append({signal.id, 0});
    for (int position = 0; position < m_lengthSpin->value() * 8; ++position) {
        const int startBit = signal.byteOrder == Signal::ByteOrder::Intel
                                 ? position
                                 : (position / 8) * 8 + 7 - position % 8;
        candidate.last().startBit = startBit;
        if (checkSignalLayout(m_lengthSpin->value(), candidate, m_signalsById).isEmpty())
            return startBit;
    }
    return -1;
}

void MessageDialog::addSignal()
{
    // Offer the signals that are not in this message yet.
    QStringList names;
    QHash<QString, int> idByName;
    for (const Signal &signal : m_signalsById) {
        bool alreadyPlaced = false;
        for (const SignalMapping &mapping : m_mappings)
            alreadyPlaced = alreadyPlaced || mapping.signalId == signal.id;
        if (!alreadyPlaced) {
            names << signal.name;
            idByName.insert(signal.name, signal.id);
        }
    }
    names.sort(Qt::CaseInsensitive);

    if (names.isEmpty()) {
        QMessageBox::information(this, windowTitle(),
                                 "There are no signals left to add. Create signals under 'Signals' first.");
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getItem(this, "Add Signal", "Signal:", names, 0, false, &ok);
    if (!ok)
        return;

    const Signal signal = m_signalsById.value(idByName.value(name));
    const int startBit = findFreeStartBit(signal);
    if (startBit < 0) {
        QMessageBox::warning(this, windowTitle(),
                             QString("There is no free room for '%1' (%2 bits) in %3 bytes.")
                                 .arg(signal.name).arg(signal.length).arg(m_lengthSpin->value()));
        return;
    }

    m_mappings.append({signal.id, startBit});
    refreshSignals();
}

void MessageDialog::removeSignal()
{
    const int row = m_signalsTable->currentRow();
    if (row < 0 || row >= m_mappings.size())
        return;

    m_mappings.removeAt(row);
    refreshSignals();
}

void MessageDialog::moveSignal(int mappingIndex, int newStartBit)
{
    m_mappings[mappingIndex].startBit = newStartBit;
    refreshSignals();
}
