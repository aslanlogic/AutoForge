#ifndef SIGNALDIALOG_H
#define SIGNALDIALOG_H

#include <QDialog>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QTabWidget;
class QWidget;

class SignalDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SignalDialog(const QString &name, QWidget *parent = nullptr);

private:
    QWidget *createDefinitionTab();
    QWidget *createValueDescriptionsTab();
    QWidget *createCommentTab();

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

};

#endif // SIGNALDIALOG_H
