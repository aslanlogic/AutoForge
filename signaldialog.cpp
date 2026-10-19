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
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

SignalDialog::SignalDialog(const QString &name, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QString("Signal '%1'").arg(name));

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

    m_nameEdit->setText(name);
    resize(640, 520);
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

    m_valueTableCombo = new QComboBox;
    m_valueTableCombo->addItem("<none>");

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

    m_valueDescriptionsTable = new QTableWidget(0, 2);
    m_valueDescriptionsTable->setHorizontalHeaderLabels({"Value", "Description"});
    m_valueDescriptionsTable->horizontalHeader()->setStretchLastSection(true);
    m_valueDescriptionsTable->verticalHeader()->setVisible(false);

    auto *layout = new QVBoxLayout(tab);
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
