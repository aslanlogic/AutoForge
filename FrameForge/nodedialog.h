#ifndef NODEDIALOG_H
#define NODEDIALOG_H

#include "node.h"

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QPlainTextEdit;

// Asks for a network node's name and comment.
class NodeDialog : public QDialog
{
    Q_OBJECT

public:
    // existingNames: names already taken by other nodes; OK is refused for these.
    NodeDialog(const Node &node, const QStringList &existingNames, QWidget *parent = nullptr);

    Node node() const;

public slots:
    void accept() override;

private:
    Node m_node; // the node as it was when the dialog opened
    QStringList m_existingNames;

    QLineEdit *m_nameEdit = nullptr;
    QPlainTextEdit *m_commentEdit = nullptr;
};

#endif // NODEDIALOG_H
