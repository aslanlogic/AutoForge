#include "nodedialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>

using namespace ff;

NodeDialog::NodeDialog(const Node &node, const QStringList &existingNames, QWidget *parent)
    : QDialog(parent)
    , m_node(node)
    , m_existingNames(existingNames)
{
    setWindowTitle(QString("Network Node '%1'").arg(node.name));

    m_nameEdit = new QLineEdit(node.name);
    m_commentEdit = new QPlainTextEdit(node.comment);

    auto *form = new QFormLayout;
    form->addRow("Name:", m_nameEdit);
    form->addRow("Comment:", m_commentEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    resize(480, 300);
}

Node NodeDialog::node() const
{
    // Start from the node we were given, so fields the dialog does not show (like id) are kept.
    Node node = m_node;
    node.name = m_nameEdit->text().trimmed();
    node.comment = m_commentEdit->toPlainText();
    return node;
}

void NodeDialog::accept()
{
    const QString name = m_nameEdit->text().trimmed();

    QString error;
    if (name.isEmpty())
        error = "Please enter a name for the network node.";
    else if (m_existingNames.contains(name))
        error = QString("A network node named '%1' already exists.").arg(name);

    if (!error.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), error);
        m_nameEdit->setFocus();
        m_nameEdit->selectAll();
        return;
    }

    QDialog::accept();
}
