#ifndef DATATYPEDIALOG_H
#define DATATYPEDIALOG_H

#include "usccore/datatype.h"
#include "usccore/valuetable.h"

#include <QDialog>
#include <QHash>
#include <QStringList>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QTabWidget;
class QWidget;

using namespace usc;

// Asks for all properties of a data type. For ASCII and Bytes the number fields
// (unit, factor, offset, minimum, maximum, value table) do not apply and are disabled.
class DataTypeDialog : public QDialog
{
    Q_OBJECT

public:
    // existingNames: names already taken by other data types; OK is refused for these.
    // allValueTables: the value tables the dataType can use.
    DataTypeDialog(const DataType &dataType, const QStringList &existingNames,
                 const QVector<ValueTable> &allValueTables, QWidget *parent = nullptr);

    DataType dataType() const;

public slots:
    void accept() override;

private:
    QWidget *createDefinitionTab();
    QWidget *createValueDescriptionsTab();
    QWidget *createCommentTab();

    DataType m_dataType; // the dataType as it was when the dialog opened
    QStringList m_existingNames;
    QHash<int, ValueTable> m_valueTablesById;
    QTabWidget *m_tabs = nullptr;

    // Definition tab
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_lengthEdit = nullptr;
    QComboBox *m_byteOrderCombo = nullptr;
    QLineEdit *m_unitEdit = nullptr;
    QComboBox *m_valueTypeCombo = nullptr;
    QLineEdit *m_factorEdit = nullptr;
    QLineEdit *m_offsetEdit = nullptr;
    QLineEdit *m_minimumEdit = nullptr;
    QLineEdit *m_maximumEdit = nullptr;
    QComboBox *m_valueTableCombo = nullptr;
    QWidget *m_calculateButton = nullptr;

    // Value Descriptions tab
    QTableWidget *m_valueDescriptionsTable = nullptr;

    // Comment tab
    QPlainTextEdit *m_commentEdit = nullptr;

private slots:
    void onValueTypeChanged(const QString &text);
    void calculateMinimumMaximum();
    void onValueTableChanged(); // shows the chosen table's values on the Value Descriptions tab

};

#endif // DATATYPEDIALOG_H
