#ifndef NETWORKDIALOG_H
#define NETWORKDIALOG_H

#include "usccore/network.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;
class QTabWidget;
class QWidget;

using namespace usc;

// Edits the network: the request/response IDs and a
// "Protocol" box that chooses the one active protocol; only that protocol's tab
// (CAN, CAN FD, UART or UDP) can be edited.
// OK is refused if the IDs do not fit the chosen settings (e.g. 0x18DA10F1 with
// a standard CAN ID, or 0x1234 with a 1-byte UART ID).
class NetworkDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NetworkDialog(const Network &network, QWidget *parent = nullptr);

    // The IDs and the chosen protocol with its settings; the other
    // protocols keep their defaults.
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

    QComboBox *m_protocolCombo = nullptr;
    QLineEdit *m_requestIdEdit = nullptr;
    QLineEdit *m_responseIdEdit = nullptr;
    QTabWidget *m_tabs = nullptr; // one tab per protocol, in Network::Protocol order

    // CAN tab
    QComboBox *m_canBitrateCombo = nullptr;
    QDoubleSpinBox *m_canSamplePointSpin = nullptr;
    QComboBox *m_canIdFormatCombo = nullptr;

    // CAN FD tab
    QComboBox *m_canFdNominalBitrateCombo = nullptr;
    QDoubleSpinBox *m_canFdNominalSamplePointSpin = nullptr;
    QComboBox *m_canFdDataBitrateCombo = nullptr;
    QDoubleSpinBox *m_canFdDataSamplePointSpin = nullptr;
    QCheckBox *m_canFdIsoCheck = nullptr;
    QComboBox *m_canFdIdFormatCombo = nullptr;

    // UART tab
    QComboBox *m_uartBaudRateCombo = nullptr;
    QComboBox *m_uartDataBitsCombo = nullptr;
    QComboBox *m_uartParityCombo = nullptr;
    QComboBox *m_uartStopBitsCombo = nullptr;
    QComboBox *m_uartFlowControlCombo = nullptr;
    QSpinBox *m_uartIdSizeSpin = nullptr;
    QComboBox *m_uartCrcCombo = nullptr;

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
