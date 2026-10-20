#ifndef MESSAGEDIALOG_H
#define MESSAGEDIALOG_H

#include "message.h"

#include <QDialog>
#include <QHash>
#include <QStringList>

class MessageLayoutWidget;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class QWidget;

class MessageDialog : public QDialog
{
    Q_OBJECT

public:
    // existingNames: names already taken by other messages; OK is refused for these.
    // allSignals: the signals that can be placed in the message.
    MessageDialog(const Message &message, const QStringList &existingNames,
                  const QVector<Signal> &allSignals, QWidget *parent = nullptr);

    Message message() const;

public slots:
    void accept() override;

private slots:
    void onTypeChanged();
    void onTxMethodChanged();
    void addSignal();
    void removeSignal();
    void moveSignal(int mappingIndex, int newStartBit);

private:
    QWidget *createDefinitionTab();
    QWidget *createSignalsTab();
    QWidget *createLayoutTab();
    QWidget *createCommentTab();
    void refreshSignals(); // redraws the Signals and Layout tabs from m_mappings
    int findFreeStartBit(const Signal &signal) const; // -1 if it does not fit anywhere
    Message::Type currentType() const;
    Message::TxMethod currentTxMethod() const;

    Message m_message; // the message as it was when the dialog opened
    QStringList m_existingNames;
    QHash<int, Signal> m_signalsById;
    QVector<SignalMapping> m_mappings; // edited here; copied into the message on OK
    QTabWidget *m_tabs = nullptr;

    // Definition tab
    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QLineEdit *m_frameIdEdit = nullptr;
    QLabel *m_lengthLabel = nullptr;
    QSpinBox *m_lengthSpin = nullptr;
    QComboBox *m_transmitterCombo = nullptr;
    QComboBox *m_txMethodCombo = nullptr;
    QSpinBox *m_cycleTimeSpin = nullptr;

    // Signals and Layout tabs
    QTableWidget *m_signalsTable = nullptr;
    QWidget *m_layoutTab = nullptr;
    MessageLayoutWidget *m_layoutWidget = nullptr;

    // Comment tab
    QPlainTextEdit *m_commentEdit = nullptr;
};

#endif // MESSAGEDIALOG_H
