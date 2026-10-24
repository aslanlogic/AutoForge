#include "dataidentifierdialog.h"
#include "dataidentifierlayoutwidget.h"

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

#include <algorithm>

using namespace usc;

namespace {

// Accepts hex with a 0x prefix ("0xF190") or plain decimal ("61840").
bool textToDid(const QString &text, quint16 *did)
{
    const QString trimmed = text.trimmed();
    bool ok = false;
    uint value = 0;
    if (trimmed.startsWith("0x", Qt::CaseInsensitive))
        value = trimmed.mid(2).toUInt(&ok, 16);
    else
        value = trimmed.toUInt(&ok, 10);
    if (!ok || value > 0xFFFF)
        return false;
    *did = quint16(value);
    return true;
}

} // namespace

DataIdentifierDialog::DataIdentifierDialog(const DataIdentifier &dataIdentifier, const QStringList &existingNames,
                                           const QVector<quint16> &usedDids, const QVector<DataType> &allDataTypes,
                                           QWidget *parent)
    : QDialog(parent)
    , m_dataIdentifier(dataIdentifier)
    , m_existingNames(existingNames)
    , m_usedDids(usedDids)
    , m_mappings(dataIdentifier.mappings)
{
    for (const DataType &dataType : allDataTypes)
        m_dataTypesById.insert(dataType.id, dataType);

    setWindowTitle(QString("DID '%1 (%2)'").arg(dataIdentifier.name, didToText(dataIdentifier.did)));

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createDefinitionTab(), "Definition");
    m_tabs->addTab(createDataTypesTab(), "Data Types");
    m_tabs->addTab(createLayoutTab(), "Layout");
    m_tabs->addTab(createCommentTab(), "Comment");

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_tabs);
    layout->addWidget(buttons);

    m_nameEdit->setText(dataIdentifier.name);
    m_didEdit->setText(didToText(dataIdentifier.did));
    m_lengthSpin->setValue(dataIdentifier.length);
    m_commentEdit->setPlainText(dataIdentifier.comment);
    refreshDataTypes();

    // The layout grid has one row per byte: redraw it when the length changes.
    connect(m_lengthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &DataIdentifierDialog::refreshDataTypes);

    resize(640, 520);
}

DataIdentifier DataIdentifierDialog::dataIdentifier() const
{
    // Start from the DID we were given, so fields the dialog does not show (like id) are kept.
    DataIdentifier dataIdentifier = m_dataIdentifier;
    dataIdentifier.name = m_nameEdit->text().trimmed();
    textToDid(m_didEdit->text(), &dataIdentifier.did);
    dataIdentifier.length = m_lengthSpin->value();
    dataIdentifier.mappings = m_mappings;
    dataIdentifier.comment = m_commentEdit->toPlainText();
    return dataIdentifier;
}

void DataIdentifierDialog::accept()
{
    const QString name = m_nameEdit->text().trimmed();
    quint16 did = 0;

    QString error;
    QWidget *badField = nullptr;
    if (name.isEmpty()) {
        error = "Please enter a name for the DID.";
        badField = m_nameEdit;
    } else if (m_existingNames.contains(name)) {
        error = QString("A DID named '%1' already exists.").arg(name);
        badField = m_nameEdit;
    } else if (!textToDid(m_didEdit->text(), &did)) {
        error = "The DID must be a number from 0x0000 to 0xFFFF.\n"
                "Write hex with 0x in front (0xF190) or plain decimal (61840).";
        badField = m_didEdit;
    } else if (m_usedDids.contains(did)) {
        error = QString("The DID %1 is already used by another DID.").arg(didToText(did));
        badField = m_didEdit;
    }

    if (!error.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), error);
        m_tabs->setCurrentIndex(0);
        badField->setFocus();
        if (auto *edit = qobject_cast<QLineEdit *>(badField))
            edit->selectAll();
        return;
    }

    // E.g. the length was made smaller after the data types were placed.
    const QString layoutError = checkDataTypeLayout(m_lengthSpin->value(), m_mappings, m_dataTypesById);
    if (!layoutError.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), layoutError);
        m_tabs->setCurrentWidget(m_dataTypesTab);
        return;
    }

    QDialog::accept();
}

QWidget *DataIdentifierDialog::createDefinitionTab()
{
    auto *tab = new QWidget;

    m_nameEdit = new QLineEdit;
    m_didEdit = new QLineEdit;
    m_lengthSpin = new QSpinBox;
    m_lengthSpin->setRange(1, MaxDataRecordLength);
    m_lengthSpin->setSuffix(" bytes");

    auto *grid = new QGridLayout;
    grid->addWidget(new QLabel("Name:"), 0, 0);
    grid->addWidget(m_nameEdit, 0, 1);
    grid->addWidget(new QLabel("DID:"), 1, 0);
    grid->addWidget(m_didEdit, 1, 1);
    grid->addWidget(new QLabel("Length:"), 2, 0);
    grid->addWidget(m_lengthSpin, 2, 1);
    grid->addWidget(new QLabel("The DID is written as hex with 0x in front (0xF190) or plain decimal.\n"
                               "The length is the fixed size of the data record."), 3, 1);

    auto *layout = new QVBoxLayout(tab);
    layout->addLayout(grid);
    layout->addStretch();

    return tab;
}

QWidget *DataIdentifierDialog::createDataTypesTab()
{
    auto *tab = new QWidget;
    m_dataTypesTab = tab;

    m_dataTypesTable = new QTableWidget(0, 5);
    m_dataTypesTable->setHorizontalHeaderLabels({"Name", "Data Type", "Start Bit", "Length [Bit]", "Byte Order"});
    m_dataTypesTable->horizontalHeader()->setStretchLastSection(true);
    m_dataTypesTable->verticalHeader()->setVisible(false);
    m_dataTypesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dataTypesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dataTypesTable->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *addButton = new QPushButton("Add...");
    auto *renameButton = new QPushButton("Rename...");
    auto *removeButton = new QPushButton("Remove");
    connect(addButton, &QPushButton::clicked, this, &DataIdentifierDialog::addDataType);
    connect(renameButton, &QPushButton::clicked, this, &DataIdentifierDialog::renameDataType);
    connect(removeButton, &QPushButton::clicked, this, &DataIdentifierDialog::removeDataType);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(addButton);
    buttons->addWidget(renameButton);
    buttons->addWidget(removeButton);
    buttons->addStretch();

    auto *layout = new QVBoxLayout(tab);
    layout->addWidget(new QLabel("Data types in the data record. The same data type may be added more "
                                 "than once; each gets its own name."));
    layout->addWidget(m_dataTypesTable);
    layout->addLayout(buttons);

    return tab;
}

QWidget *DataIdentifierDialog::createLayoutTab()
{
    m_layoutWidget = new DataIdentifierLayoutWidget;
    connect(m_layoutWidget, &DataIdentifierLayoutWidget::dataTypeMoved, this, &DataIdentifierDialog::moveDataType);

    // Long data records do not fit on screen: scroll.
    auto *scrollArea = new QScrollArea;
    scrollArea->setWidget(m_layoutWidget);
    scrollArea->setWidgetResizable(false);
    return scrollArea;
}

QWidget *DataIdentifierDialog::createCommentTab()
{
    auto *tab = new QWidget;

    m_commentEdit = new QPlainTextEdit;

    auto *layout = new QVBoxLayout(tab);
    layout->addWidget(m_commentEdit);

    return tab;
}

void DataIdentifierDialog::refreshDataTypes()
{
    m_dataTypesTable->setRowCount(m_mappings.size());
    for (int row = 0; row < m_mappings.size(); ++row) {
        const DataTypeMapping &mapping = m_mappings.at(row);
        const DataType dataType = m_dataTypesById.value(mapping.dataTypeId);
        const QStringList cells = {mapping.name, dataType.name, QString::number(mapping.startBit),
                                   QString::number(dataType.length), toString(dataType.byteOrder)};
        for (int column = 0; column < cells.size(); ++column) {
            auto *item = new QTableWidgetItem(cells.at(column));
            // Same colour as in the Layout tab, so the two are easy to match.
            item->setBackground(DataIdentifierLayoutWidget::colorFor(row));
            item->setForeground(Qt::black);
            m_dataTypesTable->setItem(row, column, item);
        }
    }

    m_layoutWidget->setLayoutData(m_lengthSpin->value(), m_mappings, m_dataTypesById);
}

int DataIdentifierDialog::findFreeStartBit(const DataType &dataType) const
{
    // Try positions from the start of the data record; the first one that fits wins.
    // For Motorola the start bit is the top bit of a byte first (7, 6, ... 0, 15, ...)
    // so that the data type is packed from the start of the record.
    QVector<DataTypeMapping> candidate = m_mappings;
    candidate.append({QString(), dataType.id, 0});
    for (int position = 0; position < m_lengthSpin->value() * 8; ++position) {
        const int startBit = dataType.byteOrder == DataType::ByteOrder::Intel
                                 ? position
                                 : (position / 8) * 8 + 7 - position % 8;
        candidate.last().startBit = startBit;
        if (checkDataTypeLayout(m_lengthSpin->value(), candidate, m_dataTypesById).isEmpty())
            return startBit;
    }
    return -1;
}

void DataIdentifierDialog::addDataType()
{
    // Every data type can be added, also one that is already placed.
    QStringList names;
    QHash<QString, int> idByName;
    for (const DataType &dataType : qAsConst(m_dataTypesById)) {
        names << dataType.name;
        idByName.insert(dataType.name, dataType.id);
    }
    names.sort(Qt::CaseInsensitive);

    if (names.isEmpty()) {
        QMessageBox::information(this, windowTitle(), "There are no data types yet. Create them under 'Data Types' first.");
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getItem(this, "Add Data Type", "Data type:", names, 0, false, &ok);
    if (!ok)
        return;

    const DataType dataType = m_dataTypesById.value(idByName.value(name));
    const int startBit = findFreeStartBit(dataType);
    if (startBit < 0) {
        QMessageBox::warning(this, windowTitle(),
                             QString("There is no free room for '%1' (%2 bits) in %3 bytes.")
                                 .arg(dataType.name).arg(dataType.length).arg(m_lengthSpin->value()));
        return;
    }

    // The placement is named after the data type; the second one gets "_2", and so on.
    QString placementName = dataType.name;
    for (int number = 2; std::any_of(m_mappings.cbegin(), m_mappings.cend(),
                                     [&](const DataTypeMapping &mapping) { return mapping.name == placementName; });
         ++number)
        placementName = QString("%1_%2").arg(dataType.name).arg(number);

    m_mappings.append({placementName, dataType.id, startBit});
    refreshDataTypes();
}

void DataIdentifierDialog::renameDataType()
{
    const int row = m_dataTypesTable->currentRow();
    if (row < 0 || row >= m_mappings.size())
        return;

    bool ok = false;
    const QString name = QInputDialog::getText(this, "Rename Data Type", "Name in this DID:", QLineEdit::Normal,
                                               m_mappings.at(row).name, &ok).trimmed();
    if (!ok || name == m_mappings.at(row).name)
        return;

    QString error;
    if (name.isEmpty())
        error = "Please enter a name.";
    for (const DataTypeMapping &mapping : qAsConst(m_mappings)) {
        if (mapping.name == name)
            error = QString("This DID already has a data type named '%1'.").arg(name);
    }
    if (!error.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }

    m_mappings[row].name = name;
    refreshDataTypes();
}

void DataIdentifierDialog::removeDataType()
{
    const int row = m_dataTypesTable->currentRow();
    if (row < 0 || row >= m_mappings.size())
        return;

    m_mappings.removeAt(row);
    refreshDataTypes();
}

void DataIdentifierDialog::moveDataType(int mappingIndex, int newStartBit)
{
    m_mappings[mappingIndex].startBit = newStartBit;
    refreshDataTypes();
}
