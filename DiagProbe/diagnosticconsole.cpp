#include "diagnosticconsole.h"
#include "decoding.h"
#include "tester.h"
#include "usccore/database.h"

#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

const int KindRole = Qt::UserRole;
const int IdRole = Qt::UserRole + 1;
const int KeyRole = Qt::UserRole;

const QColor ErrorColor(200, 30, 30);
const QColor PendingColor(190, 110, 0);

// TesterPresent is sent by the tester on its own every few seconds; the console
// leaves it out (the Trace window still shows it).
bool isTesterPresent(const QByteArray &request)
{
    return !request.isEmpty() && quint8(request[0]) == 0x3E;
}

QTreeWidgetItem *addHeading(QTreeWidgetItem *parent, const QString &text)
{
    auto *heading = new QTreeWidgetItem(parent, {text});
    QFont font = heading->font(0);
    font.setBold(true);
    heading->setFont(0, font);
    return heading;
}

QTableWidget *makeTable(const QStringList &headers)
{
    auto *table = new QTableWidget(0, headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    return table;
}

QTableWidgetItem *readOnlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QString sessionNames(const usc::Database &description, const QVector<int> &ids)
{
    QStringList names;
    for (int id : ids) {
        if (const usc::Session *session = description.findSession(id))
            names << session->name;
    }
    return names.join(", ");
}

QString levelNames(const usc::Database &description, const QVector<int> &ids)
{
    QStringList names;
    for (int id : ids) {
        if (const usc::SecurityLevel *level = description.findSecurityLevel(id))
            names << level->name;
    }
    return names.join(" or ");
}

// One prerequisite as a coloured chip: green if the ECU's current state meets it,
// red (with the NRC the ECU will answer) if not, grey while not measuring.
QString chip(const QString &text, bool met, bool known, quint8 nrc)
{
    const QString background = !known ? "#e4e4e4" : met ? "#d6f0d6" : "#f6d4d4";
    const QString foreground = !known ? "#444444" : met ? "#1d5e1d" : "#8a1c1c";
    const QString mark = !known ? QString() : met ? QString("✔ ") : QString("✘ ");
    // A chip wraps only before its NRC, so a long one cannot make the panel wide.
    QString content = (mark + text).toHtmlEscaped().replace(' ', "&nbsp;");
    if (known && !met && nrc != 0)
        content += " → NRC&nbsp;" + nrcText(nrc).toHtmlEscaped();
    return QString("<span style=\"background-color:%1; color:%2;\">&nbsp;%3&nbsp;</span>")
        .arg(background, foreground, content);
}

} // namespace

DiagnosticConsole::DiagnosticConsole(Tester *tester, QWidget *parent)
    : QWidget(parent)
    , m_tester(tester)
{
    m_requestTree = new QTreeWidget;
    m_requestTree->setHeaderHidden(true);
    connect(m_requestTree, &QTreeWidget::currentItemChanged, this, &DiagnosticConsole::showSelectedRequest);

    m_title = new QLabel;
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.15);
    m_title->setFont(titleFont);
    m_title->setWordWrap(true);
    m_prerequisites = new QLabel;
    m_prerequisites->setTextFormat(Qt::RichText);
    m_prerequisites->setWordWrap(true);

    m_valuesLabel = new QLabel("Values to write:");
    m_valueTable = makeTable({"Name", "Value", "Unit", "Allowed"});
    m_valueTable->setEditTriggers(QAbstractItemView::AllEditTriggers);
    m_valueTable->setSelectionMode(QAbstractItemView::NoSelection);

    m_sendButton = new QPushButton("Send");
    m_sendButton->setDefault(true);
    connect(m_sendButton, &QPushButton::clicked, this, &DiagnosticConsole::send);
    m_sendStatus = new QLabel;
    auto *sendRow = new QHBoxLayout;
    sendRow->addWidget(m_sendButton);
    sendRow->addWidget(m_sendStatus, 1);

    m_requestBytes = new QLabel;
    m_responseBytes = new QLabel;
    m_responseText = new QLabel;
    // Long words (NRC names, unbroken hex) must not stop the panel from getting narrower.
    for (QLabel *label : {m_title, m_prerequisites, m_requestBytes, m_responseBytes, m_responseText})
        label->setMinimumWidth(1);
    for (QLabel *label : {m_requestBytes, m_responseBytes, m_responseText}) {
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setWordWrap(true);
    }
    QFont mono("monospace");
    mono.setStyleHint(QFont::TypeWriter);
    m_requestBytes->setFont(mono);
    m_responseBytes->setFont(mono);

    m_decodedTable = makeTable({"Name", "Physical", "Unit", "Value table", "Raw"});

    auto *panel = new QWidget;
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->addWidget(m_title);
    panelLayout->addWidget(m_prerequisites);
    panelLayout->addWidget(m_valuesLabel);
    panelLayout->addWidget(m_valueTable, 3);
    panelLayout->addLayout(sendRow);
    panelLayout->addWidget(m_requestBytes);
    panelLayout->addWidget(m_responseBytes);
    panelLayout->addWidget(m_responseText);
    panelLayout->addWidget(m_decodedTable, 6);
    // Takes the free space when the tables are hidden, so the labels stay together at the top.
    panelLayout->addStretch(1);

    auto *top = new QSplitter(Qt::Horizontal);
    top->addWidget(m_requestTree);
    top->addWidget(panel);
    top->setChildrenCollapsible(false);
    top->setStretchFactor(1, 1);
    top->setSizes({250, 520});

    m_log = makeTable({"Time", "Dir", "Service", "Bytes", "Interpretation"});
    m_log->setFont(mono);
    m_log->setSelectionMode(QAbstractItemView::SingleSelection);
    m_log->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_log->horizontalHeader()->setStretchLastSection(true);
    m_log->setColumnWidth(0, 80);
    m_log->setColumnWidth(1, 40);
    m_log->setColumnWidth(2, 190);
    m_log->setColumnWidth(3, 230);
    m_log->verticalHeader()->setDefaultSectionSize(m_log->fontMetrics().height() + 4);
    connect(m_log, &QTableWidget::cellClicked, this, [this](int row) {
        selectRequest(m_log->item(row, 0)->data(KeyRole).toString());
    });

    auto *vertical = new QSplitter(Qt::Vertical);
    vertical->addWidget(top);
    vertical->addWidget(m_log);
    vertical->setChildrenCollapsible(false);
    vertical->setStretchFactor(0, 3);
    vertical->setStretchFactor(1, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(vertical);

    connect(m_tester, &Tester::stateChanged, this, [this] {
        updatePrerequisites();
        updateSendButton();
    });
    connect(m_tester, &Tester::requestSent, this, &DiagnosticConsole::onRequestSent);
    connect(m_tester, &Tester::responseReceived, this, &DiagnosticConsole::onResponseReceived);
    connect(m_tester, &Tester::requestTimedOut, this, &DiagnosticConsole::onRequestTimedOut);

    showDescription(nullptr, QString());
}

void DiagnosticConsole::showDescription(const usc::Database *description, const QString &fileName)
{
    using namespace usc;

    m_description = description;
    m_exchanges.clear();
    m_lastRecords.clear();
    m_requestTree->clear();
    if (!description) {
        new QTreeWidgetItem(m_requestTree, {"No diagnostic description loaded"});
        showSelectedRequest();
        return;
    }

    auto addRequest = [](QTreeWidgetItem *heading, const QString &text, RequestKind kind, int id) {
        auto *item = new QTreeWidgetItem(heading, {text});
        item->setData(0, KindRole, int(kind));
        item->setData(0, IdRole, id);
    };
    auto *root = new QTreeWidgetItem(m_requestTree, {fileName});

    QTreeWidgetItem *sessions = addHeading(root, "0x10 DiagnosticSessionControl");
    for (const Session &session : description->allSessions())
        addRequest(sessions, subFunctionToText(session.sessionType) + ' ' + session.name, RequestKind::Session, session.id);

    QTreeWidgetItem *resets = addHeading(root, "0x11 ECUReset");
    for (const ResetType &resetType : description->allResetTypes())
        addRequest(resets, subFunctionToText(resetType.resetType) + ' ' + resetType.name, RequestKind::Reset, resetType.id);

    QTreeWidgetItem *reads = addHeading(root, "0x22 ReadDataByIdentifier");
    for (const DataIdentifier &did : description->allDataIdentifiers()) {
        if (isReadable(did))
            addRequest(reads, didToText(did.did) + ' ' + did.name, RequestKind::Read, did.id);
    }

    QTreeWidgetItem *levels = addHeading(root, "0x27 SecurityAccess");
    for (const SecurityLevel &level : description->allSecurityLevels())
        addRequest(levels, subFunctionToText(level.requestSeed) + ' ' + level.name, RequestKind::Unlock, level.id);

    QTreeWidgetItem *writes = addHeading(root, "0x2E WriteDataByIdentifier");
    for (const DataIdentifier &did : description->allDataIdentifiers()) {
        if (isWritable(did))
            addRequest(writes, didToText(did.did) + ' ' + did.name, RequestKind::Write, did.id);
    }

    m_requestTree->expandAll();
    // Start on the first DID to read: the most common thing to do.
    m_requestTree->setCurrentItem(reads->childCount() > 0 ? reads->child(0) : root);
    showSelectedRequest();
}

bool DiagnosticConsole::selectRequest(const QString &key)
{
    if (key.isEmpty())
        return false;
    for (QTreeWidgetItemIterator it(m_requestTree); *it; ++it) {
        const Request request{RequestKind((*it)->data(0, KindRole).toInt()), (*it)->data(0, IdRole).toInt()};
        // A write and a read of the same DID share the DID; the service tells them apart.
        if (request.kind != RequestKind::None && keyOf(request) == key) {
            m_requestTree->setCurrentItem(*it);
            return true;
        }
    }
    return false;
}

DiagnosticConsole::Request DiagnosticConsole::selectedRequest() const
{
    const QTreeWidgetItem *item = m_requestTree->currentItem();
    if (!item || !m_description)
        return Request();
    return Request{RequestKind(item->data(0, KindRole).toInt()), item->data(0, IdRole).toInt()};
}

QString DiagnosticConsole::keyOf(const Request &request) const
{
    if (!m_description)
        return QString();
    switch (request.kind) {
    case RequestKind::None:
        break;
    case RequestKind::Session:
        if (const usc::Session *session = m_description->findSession(request.id))
            return "10:" + usc::subFunctionToText(session->sessionType);
        break;
    case RequestKind::Reset:
        if (const usc::ResetType *reset = m_description->findResetType(request.id))
            return "11:" + usc::subFunctionToText(reset->resetType);
        break;
    case RequestKind::Read:
    case RequestKind::Write:
        if (const usc::DataIdentifier *did = m_description->findDataIdentifier(request.id))
            return (request.kind == RequestKind::Read ? "22:" : "2E:") + usc::didToText(did->did);
        break;
    case RequestKind::Unlock:
        if (const usc::SecurityLevel *level = m_description->findSecurityLevel(request.id))
            return "27:" + usc::subFunctionToText(level->requestSeed);
        break;
    }
    return QString();
}

QString DiagnosticConsole::keyOfMessage(const QByteArray &message)
{
    if (message.isEmpty())
        return QString();
    const quint8 serviceId = quint8(message[0]);
    const quint8 subFunction = message.size() > 1 ? quint8(message[1]) & 0x7F : 0;
    switch (serviceId) {
    case 0x10:
    case 0x11:
        return QString("%1:").arg(serviceId, 2, 16, QChar('0')) + usc::subFunctionToText(subFunction);
    case 0x22:
    case 0x2E:
        if (message.size() >= 3)
            return QString(serviceId == 0x22 ? "22:" : "2E:")
                + usc::didToText(quint16((quint8(message[1]) << 8) | quint8(message[2])));
        break;
    case 0x27:
        return "27:" + usc::subFunctionToText(subFunction % 2 == 1 ? subFunction : subFunction - 1);
    }
    return QString();
}

void DiagnosticConsole::showSelectedRequest()
{
    const Request request = selectedRequest();
    m_valuesLabel->hide();
    m_valueTable->hide();
    m_valueTable->setRowCount(0);

    if (request.kind == RequestKind::None) {
        m_title->setText(m_description ? "Select a request in the tree."
                                       : "Open a diagnostic description with File → Open Diagnostic Description.");
        m_prerequisites->clear();
        m_sendButton->hide();
        m_sendStatus->clear();
        updateExchange();
        return;
    }

    const QTreeWidgetItem *item = m_requestTree->currentItem();
    m_title->setText(item->parent()->text(0) + " — " + item->text(0));
    switch (request.kind) {
    case RequestKind::Session: m_sendButton->setText("Switch session"); break;
    case RequestKind::Reset:   m_sendButton->setText("Reset ECU"); break;
    case RequestKind::Read:    m_sendButton->setText("Read"); break;
    case RequestKind::Unlock:  m_sendButton->setText("Request seed and send key"); break;
    case RequestKind::Write:   m_sendButton->setText("Write"); break;
    case RequestKind::None:    break;
    }
    m_sendButton->show();
    if (request.kind == RequestKind::Write)
        fillWriteValues(request.id);
    updatePrerequisites();
    updateSendButton();
    updateExchange();
}

void DiagnosticConsole::fillWriteValues(int dataIdentifierId)
{
    const usc::DataIdentifier *did = m_description->findDataIdentifier(dataIdentifierId);
    if (!did)
        return;
    m_valuesLabel->show();
    m_valueTable->show();

    // Start from what was read or written last, so changing one value keeps the others.
    const QByteArray record = m_lastRecords.value(did->did, QByteArray(did->length, '\0'));
    const QVector<DecodedValue> values = decodeDataRecord(*m_description, *did, record);
    m_valueTable->setRowCount(values.size());
    for (int row = 0; row < values.size(); ++row) {
        const usc::DataTypeMapping &mapping = did->mappings[row];
        const usc::DataType *dataType = m_description->findDataType(mapping.dataTypeId);
        const DecodedValue &value = values[row];
        m_valueTable->setItem(row, 0, readOnlyItem(value.name));
        m_valueTable->setItem(row, 2, readOnlyItem(value.unit));

        QString allowed;
        if (dataType->valueType == usc::DataType::ValueType::Ascii)
            allowed = QString("text, up to %1 characters").arg(dataType->length / 8);
        else if (dataType->valueType == usc::DataType::ValueType::Bytes)
            allowed = QString("%1 bytes of hex").arg(dataType->length / 8);
        else if (dataType->maximum > dataType->minimum)
            allowed = formatNumber(dataType->minimum) + " … " + formatNumber(dataType->maximum);
        m_valueTable->setItem(row, 3, readOnlyItem(allowed));

        const usc::ValueTable *table = m_description->findValueTable(dataType->valueTableId);
        if (table && !table->entries.isEmpty()) {
            auto *combo = new QComboBox;
            for (const usc::ValueTable::Entry &entry : table->entries)
                combo->addItem(QString("%1 (%2)").arg(entry.description).arg(entry.value), entry.description);
            const int index = combo->findData(value.valueText);
            combo->setCurrentIndex(qMax(0, index));
            m_valueTable->setItem(row, 1, readOnlyItem(QString()));
            m_valueTable->setCellWidget(row, 1, combo);
        } else {
            m_valueTable->setItem(row, 1, new QTableWidgetItem(value.physicalText.trimmed()));
        }
    }
    m_valueTable->resizeColumnsToContents();
    m_valueTable->setColumnWidth(1, qMax(180, m_valueTable->columnWidth(1)));
    m_valueTable->horizontalHeader()->setStretchLastSection(true);
}

void DiagnosticConsole::updatePrerequisites()
{
    const Request request = selectedRequest();
    if (request.kind == RequestKind::None || !m_description) {
        m_prerequisites->clear();
        return;
    }
    const bool known = m_tester->isRunning();
    const int session = m_tester->sessionId();
    QStringList chips;
    auto sessionChip = [&](const QVector<int> &ids, quint8 nrc) {
        chips << chip("Session: " + sessionNames(*m_description, ids), ids.contains(session), known, nrc);
    };
    auto securityChip = [&](const QVector<int> &ids) {
        if (ids.isEmpty())
            chips << chip("No security needed", true, known, 0);
        else
            chips << chip("Unlocked: " + levelNames(*m_description, ids), m_tester->isUnlocked(ids), known, 0x33);
    };

    switch (request.kind) {
    case RequestKind::Session:
        chips << chip("Allowed in every session", true, known, 0);
        break;
    case RequestKind::Reset:
        if (const usc::ResetType *reset = m_description->findResetType(request.id)) {
            sessionChip(reset->sessionIds, 0x7E);
            securityChip(reset->securityLevelIds);
        }
        break;
    case RequestKind::Read:
        if (const usc::DataIdentifier *did = m_description->findDataIdentifier(request.id)) {
            sessionChip(did->readSessionIds, 0x31);
            securityChip(did->readSecurityLevelIds);
        }
        break;
    case RequestKind::Unlock:
        if (const usc::SecurityLevel *level = m_description->findSecurityLevel(request.id)) {
            sessionChip(level->sessionIds, 0x7E);
            if (m_tester->unlockedLevelIds().contains(level->id))
                chips << chip("Already unlocked", true, known, 0);
        }
        break;
    case RequestKind::Write:
        if (const usc::DataIdentifier *did = m_description->findDataIdentifier(request.id)) {
            sessionChip(did->writeSessionIds, 0x31);
            securityChip(did->writeSecurityLevelIds);
        }
        break;
    case RequestKind::None:
        break;
    }
    QString text = chips.join(" &nbsp;"); // the chips wrap between each other
    if (!known)
        text += "<br><span style=\"color:#777777;\">Start the measurement to send requests.</span>";
    m_prerequisites->setText(text);
}

void DiagnosticConsole::updateSendButton()
{
    const bool canSend = m_tester->isRunning() && !m_tester->isBusy() && selectedRequest().kind != RequestKind::None;
    m_sendButton->setEnabled(canSend);
    if (m_tester->isBusy())
        m_sendStatus->setText("Waiting for the response…");
    else if (!m_sendStatus->text().startsWith("Cannot"))
        m_sendStatus->clear();
}

void DiagnosticConsole::updateExchange()
{
    const Exchange exchange = m_exchanges.value(keyOf(selectedRequest()));
    m_decodedTable->setRowCount(0);
    if (exchange.request.isEmpty()) {
        m_requestBytes->setText(selectedRequest().kind == RequestKind::None ? QString() : "Request:   (not sent yet)");
        m_responseBytes->clear();
        m_responseText->clear();
        m_decodedTable->hide();
        return;
    }
    m_requestBytes->setText("Request:   " + hexBytes(exchange.request));
    const bool negative = isNegativeResponse(exchange.response);
    const bool pending = negative && exchange.response.size() >= 3 && quint8(exchange.response[2]) == 0x78;
    m_responseBytes->setText("Response:  " + (exchange.response.isEmpty() ? QString("—") : hexBytes(exchange.response)));
    m_responseText->setText(exchange.note.isEmpty() ? describeUds(m_description, exchange.response) : exchange.note);
    const QString color = pending ? "color:#be6e00;"
                        : (negative || !exchange.note.isEmpty()) ? "color:#c81e1e;" : "color:#1d5e1d;";
    m_responseText->setStyleSheet(color);
    m_responseBytes->setStyleSheet(negative ? color : QString());

    // The decoded values: of the data record read, or written.
    QVector<DecodedValue> values;
    const quint8 serviceId = exchange.response.isEmpty() ? 0 : quint8(exchange.response[0]);
    if ((serviceId == 0x62 || serviceId == 0x6E) && exchange.request.size() >= 3) {
        const quint16 didNumber = quint16((quint8(exchange.request[1]) << 8) | quint8(exchange.request[2]));
        const QByteArray record = serviceId == 0x62 ? exchange.response.mid(3) : exchange.request.mid(3);
        if (const usc::DataIdentifier *did = findDid(*m_description, didNumber))
            values = decodeDataRecord(*m_description, *did, record);
    } else if (serviceId == 0x50 && exchange.response.size() == 6) {
        DecodedValue p2, p2Star;
        p2.name = "P2";
        p2.raw = QString::number((quint8(exchange.response[2]) << 8) | quint8(exchange.response[3]));
        p2.physicalText = p2.raw;
        p2.unit = "ms";
        p2Star.name = "P2*";
        p2Star.raw = QString::number((quint8(exchange.response[4]) << 8) | quint8(exchange.response[5]));
        p2Star.physicalText = QString::number(p2Star.raw.toInt() * usc::P2StarStep);
        p2Star.unit = "ms";
        values = {p2, p2Star};
    }
    m_decodedTable->setVisible(!values.isEmpty());
    m_decodedTable->setRowCount(values.size());
    for (int row = 0; row < values.size(); ++row) {
        const DecodedValue &value = values[row];
        m_decodedTable->setItem(row, 0, readOnlyItem(value.name));
        m_decodedTable->setItem(row, 1, readOnlyItem(value.physicalText));
        m_decodedTable->setItem(row, 2, readOnlyItem(value.unit));
        m_decodedTable->setItem(row, 3, readOnlyItem(value.valueText));
        m_decodedTable->setItem(row, 4, readOnlyItem(value.raw));
    }
    m_decodedTable->resizeColumnsToContents();
    m_decodedTable->horizontalHeader()->setStretchLastSection(true);
}

void DiagnosticConsole::send()
{
    const Request request = selectedRequest();
    if (!m_description || request.kind == RequestKind::None)
        return;
    m_sendStatus->clear();
    QByteArray bytes;
    switch (request.kind) {
    case RequestKind::Session:
        if (const usc::Session *session = m_description->findSession(request.id))
            bytes = bytesOf({0x10, session->sessionType});
        break;
    case RequestKind::Reset:
        if (const usc::ResetType *reset = m_description->findResetType(request.id))
            bytes = bytesOf({0x11, reset->resetType});
        break;
    case RequestKind::Read:
        if (const usc::DataIdentifier *did = m_description->findDataIdentifier(request.id))
            bytes = bytesOf({0x22, did->did >> 8, did->did});
        break;
    case RequestKind::Unlock:
        m_tester->unlock(request.id);
        return;
    case RequestKind::Write: {
        QString error;
        if (!buildWriteRequest(request.id, &bytes, &error)) {
            m_sendStatus->setText("Cannot write: " + error);
            m_sendStatus->setStyleSheet("color:#c81e1e;");
            return;
        }
        break;
    }
    case RequestKind::None:
        return;
    }
    m_sendStatus->setStyleSheet(QString());
    m_tester->sendRequest(bytes);
}

bool DiagnosticConsole::buildWriteRequest(int dataIdentifierId, QByteArray *request, QString *errorMessage) const
{
    const usc::DataIdentifier *did = m_description->findDataIdentifier(dataIdentifierId);
    if (!did)
        return false;
    QByteArray record = m_lastRecords.value(did->did, QByteArray(did->length, '\0'));
    record.resize(did->length);
    for (int row = 0; row < m_valueTable->rowCount() && row < did->mappings.size(); ++row) {
        QString text;
        if (auto *combo = qobject_cast<QComboBox *>(m_valueTable->cellWidget(row, 1)))
            text = combo->currentData().toString();
        else
            text = m_valueTable->item(row, 1)->text();
        if (!encodePlacement(*m_description, did->mappings[row], text, &record, errorMessage))
            return false;
    }
    *request = bytesOf({0x2E, did->did >> 8, did->did}) + record;
    return true;
}

void DiagnosticConsole::onRequestSent(double time, const QByteArray &request)
{
    if (isTesterPresent(request))
        return;
    const QString key = keyOfMessage(request);
    if (!key.isEmpty())
        m_exchanges.insert(key, Exchange{request, QByteArray(), QString()});
    addLogRow(time, "Tx", request, describeUds(m_description, request), key, QColor());
    if (key == keyOf(selectedRequest()))
        updateExchange();
}

void DiagnosticConsole::onResponseReceived(double time, const QByteArray &request, const QByteArray &response)
{
    if (isTesterPresent(request))
        return;
    const QString key = keyOfMessage(request);
    const bool negative = isNegativeResponse(response);
    const bool pending = negative && response.size() >= 3 && quint8(response[2]) == 0x78;
    if (!key.isEmpty())
        m_exchanges.insert(key, Exchange{request, response, QString()});
    if (!negative && request.size() >= 3) {
        const quint16 didNumber = quint16((quint8(request[1]) << 8) | quint8(request[2]));
        if (quint8(response[0]) == 0x62)
            m_lastRecords.insert(didNumber, response.mid(3));
        else if (quint8(response[0]) == 0x6E)
            m_lastRecords.insert(didNumber, request.mid(3));
    }
    addLogRow(time, "Rx", response, describeUds(m_description, response), key,
              pending ? PendingColor : negative ? ErrorColor : QColor());
    if (key == keyOf(selectedRequest()))
        updateExchange();
}

void DiagnosticConsole::onRequestTimedOut(double time, const QByteArray &request)
{
    if (isTesterPresent(request))
        return;
    const QString key = keyOfMessage(request);
    if (!key.isEmpty())
        m_exchanges.insert(key, Exchange{request, QByteArray(), "No response within P2 / P2*."});
    addLogRow(time, "—", QByteArray(), "No response to " + serviceName(quint8(request[0])) + " (timeout)", key,
              ErrorColor);
    if (key == keyOf(selectedRequest()))
        updateExchange();
}

void DiagnosticConsole::addLogRow(double time, const QString &direction, const QByteArray &bytes,
                                  const QString &interpretation, const QString &key, const QColor &color)
{
    const int maxRows = 2000;
    if (m_log->rowCount() >= maxRows)
        m_log->removeRow(0);
    const int row = m_log->rowCount();
    m_log->insertRow(row);
    QString service;
    if (!bytes.isEmpty()) {
        const quint8 serviceId = quint8(bytes[0]);
        const quint8 requestId = serviceId == 0x7F && bytes.size() > 1 ? quint8(bytes[1])
                               : serviceId >= 0x40 ? serviceId - 0x40 : serviceId;
        service = usc::subFunctionToText(requestId) + ' ' + serviceName(requestId);
    }
    const QStringList texts = {QString::number(time, 'f', 3), direction, service, hexBytes(bytes), interpretation};
    for (int column = 0; column < texts.size(); ++column) {
        auto *item = readOnlyItem(texts[column]);
        item->setToolTip(texts[column]);
        if (color.isValid())
            item->setForeground(color);
        m_log->setItem(row, column, item);
    }
    m_log->item(row, 0)->setData(KeyRole, key);
    m_log->scrollToBottom();
}
