#ifndef VALUETABLEDIALOG_H
#define VALUETABLEDIALOG_H

#include "FrameForgeCore/valuetable.h"

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QPlainTextEdit;
class QTableWidget;

using namespace ff;

// Edits a value table: its name, its value -> description entries and a comment.
class ValueTableDialog : public QDialog
{
    Q_OBJECT

public:
    // existingNames: names already taken by other value tables; OK is refused for these.
    ValueTableDialog(const ValueTable &valueTable, const QStringList &existingNames, QWidget *parent = nullptr);

    // The entries come back sorted by value.
    ValueTable valueTable() const;

public slots:
    void accept() override;

private slots:
    void addEntry();
    void removeEntries();

private:
    // Shows the problem and selects the cell that has it.
    void showError(int row, int column, const QString &error);

    ValueTable m_valueTable; // the value table as it was when the dialog opened
    QStringList m_existingNames;

    QLineEdit *m_nameEdit = nullptr;
    QTableWidget *m_entriesTable = nullptr; // columns: value, description
    QPlainTextEdit *m_commentEdit = nullptr;
};

#endif // VALUETABLEDIALOG_H
