#include "signaldialog.h"

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

using namespace ff;

SignalDialog::SignalDialog(const Signal &signal, const QStringList &existingNames,
                           const QVector<ValueTable> &allValueTables, QWidget *parent)
    : QDialog(parent)
    , m_signal(signal)
    , m_existingNames(existingNames)
{
    for (const ValueTable &valueTable : allValueTables)
        m_valueTablesById.insert(valueTable.id, valueTable);

    setWindowTitle(QString("Signal '%1'").arg(signal.name));

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

    // Combo box item order matches the enum order in Signal.
    // Value type goes before length: choosing a float type overwrites the length.
    const QLocale c = QLocale::c();
    m_nameEdit->setText(signal.name);
    m_valueTypeCombo->setCurrentIndex(static_cast<int>(signal.valueType));
    m_lengthEdit->setText(QString::number(signal.length));
    m_byteOrderCombo->setCurrentIndex(static_cast<int>(signal.byteOrder));
    m_unitEdit->setText(signal.unit);
    m_initValueEdit->setText(c.toString(signal.initialValue, 'g', QLocale::FloatingPointShortest));
    m_factorEdit->setText(c.toString(signal.factor, 'g', QLocale::FloatingPointShortest));
    m_offsetEdit->setText(c.toString(signal.offset, 'g', QLocale::FloatingPointShortest));
    m_minimumEdit->setText(c.toString(signal.minimum, 'g', QLocale::FloatingPointShortest));
    m_maximumEdit->setText(c.toString(signal.maximum, 'g', QLocale::FloatingPointShortest));
    m_commentEdit->setPlainText(signal.comment);
    m_valueTableCombo->setCurrentIndex(qMax(0, m_valueTableCombo->findData(signal.valueTableId)));
    onValueTableChanged();
    connect(m_valueTableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SignalDialog::onValueTableChanged);

    resize(640, 520);
}

Signal SignalDialog::signal() const
{
    const QLocale c = QLocale::c();
    // Start from the signal we were given, so fields the dialog does not show (like id) are kept.
    Signal signal = m_signal;
    signal.name = m_nameEdit->text().trimmed();
    signal.length = m_lengthEdit->text().toInt();
    signal.byteOrder = static_cast<Signal::ByteOrder>(m_byteOrderCombo->currentIndex());
    signal.valueType = static_cast<Signal::ValueType>(m_valueTypeCombo->currentIndex());
    signal.unit = m_unitEdit->text().trimmed();
    signal.initialValue = c.toDouble(m_initValueEdit->text());
    signal.factor = c.toDouble(m_factorEdit->text());
    signal.offset = c.toDouble(m_offsetEdit->text());
    signal.minimum = c.toDouble(m_minimumEdit->text());
    signal.maximum = c.toDouble(m_maximumEdit->text());
    signal.valueTableId = m_valueTableCombo->currentData().toInt();
    signal.comment = m_commentEdit->toPlainText();
    return signal;
}

void SignalDialog::accept()
{
    const QString name = m_nameEdit->text().trimmed();

    QString error;
    if (name.isEmpty())
        error = "Please enter a name for the signal.";
    else if (m_existingNames.contains(name))
        error = QString("A signal named '%1' already exists.").arg(name);

    if (!error.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), error);
        m_tabs->setCurrentIndex(0);
        m_nameEdit->setFocus();
        m_nameEdit->selectAll();
        return;
    }

    QDialog::accept();
}

QWidget *SignalDialog::createDefinitionTab()
{
    auto *tab = new QWidget;

    m_nameEdit = new QLineEdit;

    m_lengthEdit = new QLineEdit("8");
    m_lengthEdit->setValidator(new QIntValidator(1, 64, m_lengthEdit));

    m_byteOrderCombo = new QComboBox;
    m_byteOrderCombo->addItems({"Intel", "Motorola"});

    m_unitEdit = new QLineEdit;

    m_valueTypeCombo = new QComboBox;
    m_valueTypeCombo->addItems({"Signed", "Unsigned", "IEEE Float", "IEEE Double"});
    connect(m_valueTypeCombo, &QComboBox::currentTextChanged, this, &SignalDialog::onValueTypeChanged);

    auto makeNumberEdit = [](const QString &value) {
        auto *edit = new QLineEdit(value);
        auto *validator = new QDoubleValidator(edit);
        validator->setLocale(QLocale::c());
        edit->setValidator(validator);
        return edit;
    };
    m_initValueEdit = makeNumberEdit("0");
    m_factorEdit = makeNumberEdit("1");
    m_offsetEdit = makeNumberEdit("0");
    m_minimumEdit = makeNumberEdit("0");
    m_maximumEdit = makeNumberEdit("0");

    auto *calculateButton = new QPushButton("Calculate minimum and maximum");
    connect(calculateButton, &QPushButton::clicked, this, &SignalDialog::calculateMinimumMaximum);

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
    grid->addWidget(new QLabel("Init. Value:"), 3, 2);
    grid->addWidget(m_initValueEdit, 3, 3);
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

    // Keep the form compact at the top-left, like CANdb++.
    auto *layout = new QVBoxLayout(tab);
    auto *row = new QHBoxLayout;
    row->addLayout(grid);
    row->addStretch();
    layout->addLayout(row);
    layout->addStretch();

    return tab;
}

QWidget *SignalDialog::createValueDescriptionsTab()
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

QWidget *SignalDialog::createCommentTab()
{
    auto *tab = new QWidget;

    m_commentEdit = new QPlainTextEdit;

    auto *layout = new QVBoxLayout(tab);
    layout->addWidget(m_commentEdit);

    return tab;
}

void SignalDialog::onValueTypeChanged(const QString &text)
{
    // "Signed", "Unsigned", "IEEE Float", "IEEE Double"
    if(text == "IEEE Float") {
        m_lengthEdit->setText("32");
        m_lengthEdit->setDisabled(true);
    } else if(text == "IEEE Double") {
        m_lengthEdit->setText("64");
        m_lengthEdit->setDisabled(true);
    } else {
        m_lengthEdit->setDisabled(false);
    }
}

void SignalDialog::calculateMinimumMaximum()
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

void SignalDialog::onValueTableChanged()
{
    const ValueTable valueTable = m_valueTablesById.value(m_valueTableCombo->currentData().toInt());
    m_valueDescriptionsTable->setRowCount(valueTable.entries.size());
    for (int row = 0; row < valueTable.entries.size(); ++row) {
        const ValueTable::Entry &entry = valueTable.entries.at(row);
        m_valueDescriptionsTable->setItem(row, 0, new QTableWidgetItem(QString::number(entry.value)));
        m_valueDescriptionsTable->setItem(row, 1, new QTableWidgetItem(entry.description));
    }
}
