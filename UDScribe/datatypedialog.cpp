#include "datatypedialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace usc;

DataTypeDialog::DataTypeDialog(const DataType &dataType, const QStringList &existingNames,
                           const QVector<ValueTable> &allValueTables, QWidget *parent)
    : QDialog(parent)
    , m_dataType(dataType)
    , m_existingNames(existingNames)
{
    for (const ValueTable &valueTable : allValueTables)
        m_valueTablesById.insert(valueTable.id, valueTable);

    setWindowTitle(QString("Data Type '%1'").arg(dataType.name));

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createDefinitionTab(), "Definition");
    m_tabs->addTab(createValueDescriptionsTab(), "Value Descriptions");
    m_tabs->addTab(createCommentTab(), "Comment");

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_tabs);
    layout->addWidget(buttons);

    // Combo box item order matches the enum order in DataType.
    // Value type goes before length: choosing a float type overwrites the length.
    const QLocale c = QLocale::c();
    m_nameEdit->setText(dataType.name);
    m_valueTypeCombo->setCurrentIndex(static_cast<int>(dataType.valueType));
    m_lengthEdit->setText(QString::number(dataType.length));
    m_byteOrderCombo->setCurrentIndex(static_cast<int>(dataType.byteOrder));
    m_unitEdit->setText(dataType.unit);
    m_factorEdit->setText(c.toString(dataType.factor, 'g', QLocale::FloatingPointShortest));
    m_offsetEdit->setText(c.toString(dataType.offset, 'g', QLocale::FloatingPointShortest));
    m_minimumEdit->setText(c.toString(dataType.minimum, 'g', QLocale::FloatingPointShortest));
    m_maximumEdit->setText(c.toString(dataType.maximum, 'g', QLocale::FloatingPointShortest));
    m_commentEdit->setPlainText(dataType.comment);
    m_valueTableCombo->setCurrentIndex(qMax(0, m_valueTableCombo->findData(dataType.valueTableId)));
    onValueTableChanged();
    connect(m_valueTableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DataTypeDialog::onValueTableChanged);

    resize(640, 520);
}

DataType DataTypeDialog::dataType() const
{
    const QLocale c = QLocale::c();
    // Start from the data type we were given, so fields the dialog does not show (like id) are kept.
    DataType dataType = m_dataType;
    dataType.name = m_nameEdit->text().trimmed();
    dataType.length = m_lengthEdit->text().toInt();
    dataType.byteOrder = static_cast<DataType::ByteOrder>(m_byteOrderCombo->currentIndex());
    dataType.valueType = static_cast<DataType::ValueType>(m_valueTypeCombo->currentIndex());
    // The number fields only apply to numbers; text and raw bytes keep the defaults.
    if (!needsWholeBytes(dataType.valueType)) {
        dataType.unit = m_unitEdit->text().trimmed();
        dataType.factor = c.toDouble(m_factorEdit->text());
        dataType.offset = c.toDouble(m_offsetEdit->text());
        dataType.minimum = c.toDouble(m_minimumEdit->text());
        dataType.maximum = c.toDouble(m_maximumEdit->text());
        dataType.valueTableId = m_valueTableCombo->currentData().toInt();
    } else {
        const DataType defaults;
        dataType.unit = defaults.unit;
        dataType.factor = defaults.factor;
        dataType.offset = defaults.offset;
        dataType.minimum = defaults.minimum;
        dataType.maximum = defaults.maximum;
        dataType.valueTableId = defaults.valueTableId;
    }
    dataType.comment = m_commentEdit->toPlainText();
    return dataType;
}

void DataTypeDialog::accept()
{
    const QString name = m_nameEdit->text().trimmed();

    QLineEdit *badField = m_nameEdit;
    QString error;
    if (name.isEmpty()) {
        error = "Please enter a name for the data type.";
    } else if (m_existingNames.contains(name)) {
        error = QString("A data type named '%1' already exists.").arg(name);
    } else {
        error = checkLength(dataType());
        badField = m_lengthEdit;
    }

    if (!error.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), error);
        m_tabs->setCurrentIndex(0);
        badField->setFocus();
        badField->selectAll();
        return;
    }

    QDialog::accept();
}

QWidget *DataTypeDialog::createDefinitionTab()
{
    auto *tab = new QWidget;

    m_nameEdit = new QLineEdit;

    m_lengthEdit = new QLineEdit("8");
    m_lengthEdit->setValidator(new QIntValidator(1, MaxDataRecordLength * 8, m_lengthEdit));

    m_byteOrderCombo = new QComboBox;
    m_byteOrderCombo->addItems({"Intel", "Motorola"});

    m_unitEdit = new QLineEdit;

    m_valueTypeCombo = new QComboBox;
    for (auto valueType : {DataType::ValueType::Signed, DataType::ValueType::Unsigned, DataType::ValueType::IeeeFloat,
                           DataType::ValueType::IeeeDouble, DataType::ValueType::Ascii, DataType::ValueType::Bytes})
        m_valueTypeCombo->addItem(toString(valueType));
    connect(m_valueTypeCombo, &QComboBox::currentTextChanged, this, &DataTypeDialog::onValueTypeChanged);

    auto makeNumberEdit = [](const QString &value) {
        auto *edit = new QLineEdit(value);
        auto *validator = new QDoubleValidator(edit);
        validator->setLocale(QLocale::c());
        edit->setValidator(validator);
        return edit;
    };
    m_factorEdit = makeNumberEdit("1");
    m_offsetEdit = makeNumberEdit("0");
    m_minimumEdit = makeNumberEdit("0");
    m_maximumEdit = makeNumberEdit("0");

    auto *calculateButton = new QPushButton("Calculate minimum and maximum");
    connect(calculateButton, &QPushButton::clicked, this, &DataTypeDialog::calculateMinimumMaximum);
    m_calculateButton = calculateButton;

    // Item data is the ValueTable::id; 0 means none. Listed A-Z.
    m_valueTableCombo = new QComboBox;
    m_valueTableCombo->addItem("<none>", 0);
    QVector<ValueTable> valueTables = m_valueTablesById.values().toVector();
    std::sort(valueTables.begin(), valueTables.end(), [](const ValueTable &a, const ValueTable &b) {
        return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
    });
    for (const ValueTable &valueTable : qAsConst(valueTables))
        m_valueTableCombo->addItem(valueTable.name, valueTable.id);

    auto *grid = new QGridLayout;
    grid->addWidget(new QLabel("Name:"), 0, 0);
    grid->addWidget(m_nameEdit, 0, 1, 1, 3);
    grid->addWidget(new QLabel("Length [Bit]:"), 1, 0);
    grid->addWidget(m_lengthEdit, 1, 1);
    grid->addWidget(new QLabel("Byte Order:"), 2, 0);
    grid->addWidget(m_byteOrderCombo, 2, 1);
    grid->addWidget(new QLabel("Unit:"), 2, 2);
    grid->addWidget(m_unitEdit, 2, 3);
    grid->addWidget(new QLabel("Value Type:"), 3, 0);
    grid->addWidget(m_valueTypeCombo, 3, 1);
    grid->addWidget(new QLabel("Factor:"), 4, 0);
    grid->addWidget(m_factorEdit, 4, 1);
    grid->addWidget(new QLabel("Offset:"), 4, 2);
    grid->addWidget(m_offsetEdit, 4, 3);
    grid->addWidget(new QLabel("Minimum:"), 5, 0);
    grid->addWidget(m_minimumEdit, 5, 1);
    grid->addWidget(new QLabel("Maximum:"), 5, 2);
    grid->addWidget(m_maximumEdit, 5, 3);
    grid->addWidget(calculateButton, 6, 1, 1, 3);
    grid->addWidget(new QLabel("Value Table:"), 7, 0);
    grid->addWidget(m_valueTableCombo, 7, 1, 1, 3);

    // Keep the form compact at the top-left.
    auto *layout = new QVBoxLayout(tab);
    auto *row = new QHBoxLayout;
    row->addLayout(grid);
    row->addStretch();
    layout->addLayout(row);
    layout->addStretch();

    return tab;
}

QWidget *DataTypeDialog::createValueDescriptionsTab()
{
    auto *tab = new QWidget;

    // Read-only: the values come from the value table chosen on the Definition tab.
    m_valueDescriptionsTable = new QTableWidget(0, 2);
    m_valueDescriptionsTable->setHorizontalHeaderLabels({"Value", "Description"});
    m_valueDescriptionsTable->horizontalHeader()->setStretchLastSection(true);
    m_valueDescriptionsTable->verticalHeader()->setVisible(false);
    m_valueDescriptionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto *layout = new QVBoxLayout(tab);
    layout->addWidget(new QLabel("The values of the value table chosen on the Definition tab "
                                 "(edit the table itself under 'Value Tables'):"));
    layout->addWidget(m_valueDescriptionsTable);

    return tab;
}

QWidget *DataTypeDialog::createCommentTab()
{
    auto *tab = new QWidget;

    m_commentEdit = new QPlainTextEdit;

    auto *layout = new QVBoxLayout(tab);
    layout->addWidget(m_commentEdit);

    return tab;
}

void DataTypeDialog::onValueTypeChanged(const QString &text)
{
    // Floats have a fixed length.
    if (text == toString(DataType::ValueType::IeeeFloat)) {
        m_lengthEdit->setText("32");
        m_lengthEdit->setDisabled(true);
    } else if (text == toString(DataType::ValueType::IeeeDouble)) {
        m_lengthEdit->setText("64");
        m_lengthEdit->setDisabled(true);
    } else {
        m_lengthEdit->setDisabled(false);
    }

    // Text and raw bytes have no physical value: no unit, scaling, range or value table.
    const bool isNumber = !needsWholeBytes(static_cast<DataType::ValueType>(m_valueTypeCombo->currentIndex()));
    for (QWidget *field : {static_cast<QWidget *>(m_unitEdit), static_cast<QWidget *>(m_factorEdit),
                           static_cast<QWidget *>(m_offsetEdit), static_cast<QWidget *>(m_minimumEdit),
                           static_cast<QWidget *>(m_maximumEdit), m_calculateButton,
                           static_cast<QWidget *>(m_valueTableCombo)})
        field->setEnabled(isNumber);
}

void DataTypeDialog::calculateMinimumMaximum()
{
    const QLocale c = QLocale::c();
    bool lengthOk = false, factorOk = false, offsetOk = false;
    const int length = m_lengthEdit->text().toInt(&lengthOk);
    const double factor = c.toDouble(m_factorEdit->text(), &factorOk);
    const double offset = c.toDouble(m_offsetEdit->text(), &offsetOk);
    if (!lengthOk || !factorOk || !offsetOk || length < 1 || length > 64)
        return;

    // Range of the raw value as stored in the frame.
    double rawMin = 0.0;
    double rawMax = 0.0;
    const QString valueType = m_valueTypeCombo->currentText();
    if (valueType == "Signed") {
        rawMin = -std::ldexp(1.0, length - 1);
        rawMax = std::ldexp(1.0, length - 1) - 1.0;
    } else if (valueType == "Unsigned") {
        rawMin = 0.0;
        rawMax = std::ldexp(1.0, length) - 1.0;
    } else if (valueType == "IEEE Float") {
        rawMin = -std::numeric_limits<float>::max();
        rawMax = std::numeric_limits<float>::max();
    } else if (valueType == "IEEE Double") {
        rawMin = -std::numeric_limits<double>::max();
        rawMax = std::numeric_limits<double>::max();
    }

    // physical = raw * factor + offset; a negative factor swaps the ends.
    const double a = rawMin * factor + offset;
    const double b = rawMax * factor + offset;
    m_minimumEdit->setText(c.toString(std::min(a, b), 'g', QLocale::FloatingPointShortest));
    m_maximumEdit->setText(c.toString(std::max(a, b), 'g', QLocale::FloatingPointShortest));
}

void DataTypeDialog::onValueTableChanged()
{
    const ValueTable valueTable = m_valueTablesById.value(m_valueTableCombo->currentData().toInt());
    m_valueDescriptionsTable->setRowCount(valueTable.entries.size());
    for (int row = 0; row < valueTable.entries.size(); ++row) {
        const ValueTable::Entry &entry = valueTable.entries.at(row);
        m_valueDescriptionsTable->setItem(row, 0, new QTableWidgetItem(QString::number(entry.value)));
        m_valueDescriptionsTable->setItem(row, 1, new QTableWidgetItem(entry.description));
    }
}
