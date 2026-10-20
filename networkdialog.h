#ifndef NETWORKDIALOG_H
#define NETWORKDIALOG_H

#include "message.h"
#include "network.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;
class QTabWidget;
class QWidget;

// Edits the network: a "Protocol" box chooses the one active protocol, and
// only that protocol's tab (CAN, CAN FD, UART or UDP) can be edited.
// OK is refused if an existing message would not fit the new settings
// (e.g. a 64-byte message on CAN, or ID 0x1234 with a 1-byte UART ID).
class NetworkDialog : public QDialog
{
    Q_OBJECT

public:
    // messages: the existing messages, which all have to fit the chosen settings.
    NetworkDialog(const Network &network, const QVector<Message> &messages, QWidget *parent = nullptr);

    // The chosen protocol with its settings; the other protocols keep their defaults.
    Network network() const;

public slots:
    void accept() override;

private slots:
    void onProtocolChanged();
    void onUdpModeChanged();

private:
    QWidget *createCanTab();
    QWidget *createCanFdTab();
    QWidget *createUartTab();
    QWidget *createUdpTab();
    Network::Protocol currentProtocol() const;
    // Shows the problem and puts the cursor in the field that has it.
    void showError(QWidget *field, const QString &error);

    QVector<Message> m_messages;
    QComboBox *m_protocolCombo = nullptr;
    QTabWidget *m_tabs = nullptr; // one tab per protocol, in Network::Protocol order

    // CAN tab
    QComboBox *m_canBitrateCombo = nullptr;
    QDoubleSpinBox *m_canSamplePointSpin = nullptr;

    // CAN FD tab
    QComboBox *m_canFdNominalBitrateCombo = nullptr;
    QDoubleSpinBox *m_canFdNominalSamplePointSpin = nullptr;
    QComboBox *m_canFdDataBitrateCombo = nullptr;
    QDoubleSpinBox *m_canFdDataSamplePointSpin = nullptr;
    QCheckBox *m_canFdIsoCheck = nullptr;

    // UART tab
    QComboBox *m_uartBaudRateCombo = nullptr;
    QComboBox *m_uartDataBitsCombo = nullptr;
    QComboBox *m_uartParityCombo = nullptr;
    QComboBox *m_uartStopBitsCombo = nullptr;
    QComboBox *m_uartFlowControlCombo = nullptr;
    QLineEdit *m_uartStartBytesEdit = nullptr;
    QSpinBox *m_uartIdSizeSpin = nullptr;
    QComboBox *m_uartChecksumCombo = nullptr;

    // UDP tab
    QComboBox *m_udpModeCombo = nullptr;
    QLineEdit *m_udpLocalAddressEdit = nullptr;
    QSpinBox *m_udpLocalPortSpin = nullptr;
    QLineEdit *m_udpRemoteAddressEdit = nullptr;
    QSpinBox *m_udpRemotePortSpin = nullptr;
    QLineEdit *m_udpMulticastGroupEdit = nullptr;
    QSpinBox *m_udpIdSizeSpin = nullptr;
};

#endif // NETWORKDIALOG_H
