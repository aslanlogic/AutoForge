#ifndef SIGNALDIALOG_H
#define SIGNALDIALOG_H

#include "FrameForgeCore/signal.h"
#include "FrameForgeCore/valuetable.h"

#include <QDialog>
#include <QHash>
#include <QStringList>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QTabWidget;
class QWidget;

using namespace ff;

class SignalDialog : public QDialog
{
    Q_OBJECT

public:
    // existingNames: names already taken by other signals; OK is refused for these.
    // allValueTables: the value tables the signal can use.
    SignalDialog(const Signal &signal, const QStringList &existingNames,
                 const QVector<ValueTable> &allValueTables, QWidget *parent = nullptr);

    Signal signal() const;

public slots:
    void accept() override;

private:
    QWidget *createDefinitionTab();
    QWidget *createValueDescriptionsTab();
    QWidget *createCommentTab();

    Signal m_signal; // the signal as it was when the dialog opened
    QStringList m_existingNames;
    QHash<int, ValueTable> m_valueTablesById;
    QTabWidget *m_tabs = nullptr;

    // Definition tab
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_lengthEdit = nullptr;
    QComboBox *m_byteOrderCombo = nullptr;
    QLineEdit *m_unitEdit = nullptr;
    QComboBox *m_valueTypeCombo = nullptr;
    QLineEdit *m_initValueEdit = nullptr;
    QLineEdit *m_factorEdit = nullptr;
    QLineEdit *m_offsetEdit = nullptr;
    QLineEdit *m_minimumEdit = nullptr;
    QLineEdit *m_maximumEdit = nullptr;
    QComboBox *m_valueTableCombo = nullptr;

    // Value Descriptions tab
    QTableWidget *m_valueDescriptionsTable = nullptr;

    // Comment tab
    QPlainTextEdit *m_commentEdit = nullptr;

private slots:
    void onValueTypeChanged(const QString &text);
    void calculateMinimumMaximum();
    void onValueTableChanged(); // shows the chosen table's values on the Value Descriptions tab

};

#endif // SIGNALDIALOG_H
