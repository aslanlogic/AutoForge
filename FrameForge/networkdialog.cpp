#include "networkdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace ff;

namespace {

// A bitrate box: pick a common value or type any whole number.
QComboBox *createBitrateCombo(const QList<int> &presets)
{
    auto *combo = new QComboBox;
    combo->setEditable(true);
    for (int bitrate : presets)
        combo->addItem(QString::number(bitrate));
    combo->setValidator(new QIntValidator(1, 100000000, combo));
    return combo;
}

// 0 if the text is not a whole number.
int bitrateOf(const QComboBox *combo)
{
    return combo->currentText().trimmed().toInt();
}

QDoubleSpinBox *createSamplePointSpin()
{
    auto *spin = new QDoubleSpinBox;
    spin->setRange(50.0, 95.0);
    spin->setDecimals(1);
    spin->setSingleStep(0.5);
    spin->setSuffix(" %");
    return spin;
}

QSpinBox *createSpin(int minimum, int maximum, const QString &suffix = QString())
{
    auto *spin = new QSpinBox;
    spin->setRange(minimum, maximum);
    spin->setSuffix(suffix);
    return spin;
}

// "192.168.0.10": four numbers from 0 to 255.
bool isIpv4Address(const QString &text)
{
    static const QRegularExpression pattern(
        "^((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)\\.){3}(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)$");
    return pattern.match(text.trimmed()).hasMatch();
}

// Multicast addresses are 224.0.0.0 to 239.255.255.255.
bool isMulticastAddress(const QString &text)
{
    if (!isIpv4Address(text))
        return false;
    const int firstNumber = text.trimmed().section('.', 0, 0).toInt();
    return firstNumber >= 224 && firstNumber <= 239;
}

} // namespace

NetworkDialog::NetworkDialog(const Network &network, const QVector<Message> &messages, QWidget *parent)
    : QDialog(parent)
    , m_messages(messages)
{
    setWindowTitle("Network");

    m_protocolCombo = new QComboBox;
    for (auto protocol : {Network::Protocol::Can, Network::Protocol::CanFd,
                          Network::Protocol::Uart, Network::Protocol::Udp})
        m_protocolCombo->addItem(toString(protocol));

    // Tab order matches the Network::Protocol order, so tab index == protocol.
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createCanTab(), "CAN");
    m_tabs->addTab(createCanFdTab(), "CAN FD");
    m_tabs->addTab(createUartTab(), "UART");
    m_tabs->addTab(createUdpTab(), "UDP");

    auto *protocolForm = new QFormLayout;
    protocolForm->addRow("Protocol:", m_protocolCombo);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(protocolForm);
    layout->addWidget(m_tabs);
    layout->addWidget(buttons);

    // Fill in the current settings. Combo box item order matches the enum order in network.h.
    m_canBitrateCombo->setCurrentText(QString::number(network.can.bitrate));
    m_canSamplePointSpin->setValue(network.can.samplePoint);

    m_canFdNominalBitrateCombo->setCurrentText(QString::number(network.canFd.nominalBitrate));
    m_canFdNominalSamplePointSpin->setValue(network.canFd.nominalSamplePoint);
    m_canFdDataBitrateCombo->setCurrentText(QString::number(network.canFd.dataBitrate));
    m_canFdDataSamplePointSpin->setValue(network.canFd.dataSamplePoint);
    m_canFdIsoCheck->setChecked(network.canFd.iso);

    m_uartBaudRateCombo->setCurrentText(QString::number(network.uart.baudRate));
    m_uartDataBitsCombo->setCurrentText(QString::number(network.uart.dataBits));
    m_uartParityCombo->setCurrentIndex(static_cast<int>(network.uart.parity));
    m_uartStopBitsCombo->setCurrentIndex(static_cast<int>(network.uart.stopBits));
    m_uartFlowControlCombo->setCurrentIndex(static_cast<int>(network.uart.flowControl));
    m_uartIdSizeSpin->setValue(network.uart.idSize);
    m_uartCrcCombo->setCurrentIndex(static_cast<int>(network.uart.crc));

    m_udpModeCombo->setCurrentIndex(static_cast<int>(network.udp.mode));
    m_udpLocalAddressEdit->setText(network.udp.localAddress);
    m_udpLocalPortSpin->setValue(network.udp.localPort);
    m_udpRemoteAddressEdit->setText(network.udp.remoteAddress);
    m_udpRemotePortSpin->setValue(network.udp.remotePort);
    m_udpMulticastGroupEdit->setText(network.udp.multicastGroup);
    m_udpIdSizeSpin->setValue(network.udp.idSize);
    onUdpModeChanged();

    m_protocolCombo->setCurrentIndex(static_cast<int>(network.protocol));
    onProtocolChanged();
    connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NetworkDialog::onProtocolChanged);

    resize(560, 500);
}

Network::Protocol NetworkDialog::currentProtocol() const
{
    return static_cast<Network::Protocol>(m_protocolCombo->currentIndex());
}

Network NetworkDialog::network() const
{
    // Start from the defaults; only the chosen protocol's settings are taken from the dialog.
    Network network;
    network.protocol = currentProtocol();

    switch (network.protocol) {
    case Network::Protocol::Can:
        network.can.bitrate = bitrateOf(m_canBitrateCombo);
        network.can.samplePoint = m_canSamplePointSpin->value();
        break;
    case Network::Protocol::CanFd:
        network.canFd.nominalBitrate = bitrateOf(m_canFdNominalBitrateCombo);
        network.canFd.nominalSamplePoint = m_canFdNominalSamplePointSpin->value();
        network.canFd.dataBitrate = bitrateOf(m_canFdDataBitrateCombo);
        network.canFd.dataSamplePoint = m_canFdDataSamplePointSpin->value();
        network.canFd.iso = m_canFdIsoCheck->isChecked();
        break;
    case Network::Protocol::Uart:
        network.uart.baudRate = bitrateOf(m_uartBaudRateCombo);
        network.uart.dataBits = m_uartDataBitsCombo->currentText().toInt();
        network.uart.parity = static_cast<UartSettings::Parity>(m_uartParityCombo->currentIndex());
        network.uart.stopBits = static_cast<UartSettings::StopBits>(m_uartStopBitsCombo->currentIndex());
        network.uart.flowControl = static_cast<UartSettings::FlowControl>(m_uartFlowControlCombo->currentIndex());
        network.uart.idSize = m_uartIdSizeSpin->value();
        network.uart.crc = static_cast<UartSettings::Crc>(m_uartCrcCombo->currentIndex());
        break;
    case Network::Protocol::Udp:
        network.udp.mode = static_cast<UdpSettings::Mode>(m_udpModeCombo->currentIndex());
        network.udp.localAddress = m_udpLocalAddressEdit->text().trimmed();
        network.udp.localPort = m_udpLocalPortSpin->value();
        network.udp.remoteAddress = m_udpRemoteAddressEdit->text().trimmed();
        network.udp.remotePort = m_udpRemotePortSpin->value();
        network.udp.multicastGroup = m_udpMulticastGroupEdit->text().trimmed();
        network.udp.idSize = m_udpIdSizeSpin->value();
        break;
    }
    return network;
}

void NetworkDialog::accept()
{
    // Only the chosen protocol is checked: the other tabs are not used.
    const Network network = this->network();

    switch (network.protocol) {
    case Network::Protocol::Can:
        // Classic CAN goes up to 1 Mbit/s.
        if (network.can.bitrate < 1 || network.can.bitrate > 1000000) {
            showError(m_canBitrateCombo, "The CAN bitrate must be from 1 to 1000000 bit/s.");
            return;
        }
        break;
    case Network::Protocol::CanFd:
        if (network.canFd.nominalBitrate < 1 || network.canFd.nominalBitrate > 1000000) {
            showError(m_canFdNominalBitrateCombo, "The nominal bitrate must be from 1 to 1000000 bit/s.");
            return;
        }
        // The data phase is the fast one.
        if (network.canFd.dataBitrate < network.canFd.nominalBitrate || network.canFd.dataBitrate > 15000000) {
            showError(m_canFdDataBitrateCombo,
                      "The data bitrate must be at least the nominal bitrate and at most 15000000 bit/s.");
            return;
        }
        break;
    case Network::Protocol::Uart:
        if (network.uart.baudRate < 1) {
            showError(m_uartBaudRateCombo, "Please enter the baud rate.");
            return;
        }
        break;
    case Network::Protocol::Udp:
        if (!isIpv4Address(network.udp.localAddress)) {
            showError(m_udpLocalAddressEdit, "The local address must be an IPv4 address, e.g. 0.0.0.0 or 192.168.0.5.");
            return;
        }
        if (network.udp.mode == UdpSettings::Mode::Unicast && !isIpv4Address(network.udp.remoteAddress)) {
            showError(m_udpRemoteAddressEdit, "The remote address must be an IPv4 address, e.g. 192.168.0.10.");
            return;
        }
        if (network.udp.mode == UdpSettings::Mode::Multicast && !isMulticastAddress(network.udp.multicastGroup)) {
            showError(m_udpMulticastGroupEdit,
                      "The multicast group must be an address from 224.0.0.0 to 239.255.255.255.");
            return;
        }
        break;
    }

    // Every existing message has to fit the new settings.
    QStringList problems;
    for (const Message &message : qAsConst(m_messages)) {
        const QString problem = checkMessageAgainstNetwork(message, network);
        if (!problem.isEmpty())
            problems << problem;
    }
    if (!problems.isEmpty()) {
        const int shown = qMin(problems.size(), 10);
        QString text = "These messages do not fit the new network settings:\n\n"
                       + problems.mid(0, shown).join("\n");
        if (problems.size() > shown)
            text += QString("\n... and %1 more.").arg(problems.size() - shown);
        text += "\n\nChange the messages first, or keep the old settings.";
        showError(m_protocolCombo, text);
        return;
    }

    QDialog::accept();
}

void NetworkDialog::showError(QWidget *field, const QString &error)
{
    QMessageBox::warning(this, windowTitle(), error);
    field->setFocus();
    if (auto *edit = qobject_cast<QLineEdit *>(field))
        edit->selectAll();
}

void NetworkDialog::onProtocolChanged()
{
    // Only the chosen protocol's tab can be used; show it.
    const int active = static_cast<int>(currentProtocol());
    for (int tab = 0; tab < m_tabs->count(); ++tab)
        m_tabs->setTabEnabled(tab, tab == active);
    m_tabs->setCurrentIndex(active);
}

QWidget *NetworkDialog::createCanTab()
{
    auto *tab = new QWidget;

    m_canBitrateCombo = createBitrateCombo({125000, 250000, 500000, 1000000});
    m_canSamplePointSpin = createSamplePointSpin();

    auto *form = new QFormLayout(tab);
    form->addRow("Bitrate [bit/s]:", m_canBitrateCombo);
    form->addRow("Sample point:", m_canSamplePointSpin);
    return tab;
}

QWidget *NetworkDialog::createCanFdTab()
{
    auto *tab = new QWidget;

    m_canFdNominalBitrateCombo = createBitrateCombo({125000, 250000, 500000, 1000000});
    m_canFdNominalSamplePointSpin = createSamplePointSpin();
    m_canFdDataBitrateCombo = createBitrateCombo({1000000, 2000000, 4000000, 5000000, 8000000});
    m_canFdDataSamplePointSpin = createSamplePointSpin();
    m_canFdIsoCheck = new QCheckBox("ISO CAN FD (uncheck for the older non-ISO variant)");

    // The arbitration phase (ID and control bits) and the data phase (payload) have their own timing.
    auto *nominalGroup = new QGroupBox("Arbitration phase");
    auto *nominalForm = new QFormLayout(nominalGroup);
    nominalForm->addRow("Nominal bitrate [bit/s]:", m_canFdNominalBitrateCombo);
    nominalForm->addRow("Sample point:", m_canFdNominalSamplePointSpin);

    auto *dataGroup = new QGroupBox("Data phase");
    auto *dataForm = new QFormLayout(dataGroup);
    dataForm->addRow("Data bitrate [bit/s]:", m_canFdDataBitrateCombo);
    dataForm->addRow("Sample point:", m_canFdDataSamplePointSpin);

    auto *layout = new QVBoxLayout(tab);
    layout->addWidget(nominalGroup);
    layout->addWidget(dataGroup);
    layout->addWidget(m_canFdIsoCheck);
    layout->addStretch();
    return tab;
}

QWidget *NetworkDialog::createUartTab()
{
    auto *tab = new QWidget;

    m_uartBaudRateCombo = createBitrateCombo({9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600});
    m_uartDataBitsCombo = new QComboBox;
    m_uartDataBitsCombo->addItems({"5", "6", "7", "8"});
    m_uartParityCombo = new QComboBox;
    for (auto parity : {UartSettings::Parity::None, UartSettings::Parity::Even, UartSettings::Parity::Odd,
                        UartSettings::Parity::Mark, UartSettings::Parity::Space})
        m_uartParityCombo->addItem(toString(parity));
    m_uartStopBitsCombo = new QComboBox;
    for (auto stopBits : {UartSettings::StopBits::One, UartSettings::StopBits::OneAndHalf,
                          UartSettings::StopBits::Two})
        m_uartStopBitsCombo->addItem(toString(stopBits));
    m_uartFlowControlCombo = new QComboBox;
    for (auto flowControl : {UartSettings::FlowControl::None, UartSettings::FlowControl::Hardware,
                             UartSettings::FlowControl::Software})
        m_uartFlowControlCombo->addItem(toString(flowControl));

    m_uartIdSizeSpin = createSpin(1, 4, " byte(s)");
    m_uartCrcCombo = new QComboBox;
    for (auto crc : {UartSettings::Crc::Crc8, UartSettings::Crc::Crc16, UartSettings::Crc::Crc32})
        m_uartCrcCombo->addItem(toString(crc));

    auto *lineGroup = new QGroupBox("Serial line");
    auto *lineForm = new QFormLayout(lineGroup);
    lineForm->addRow("Baud rate:", m_uartBaudRateCombo);
    lineForm->addRow("Data bits:", m_uartDataBitsCombo);
    lineForm->addRow("Parity:", m_uartParityCombo);
    lineForm->addRow("Stop bits:", m_uartStopBitsCombo);
    lineForm->addRow("Flow control:", m_uartFlowControlCombo);

    // A serial line is just a stream of bytes; this is how a message is found in it.
    auto *frameGroup = new QGroupBox("Frame format");
    auto *frameForm = new QFormLayout(frameGroup);
    frameForm->addRow(new QLabel("Every message is sent as [ID] [DLC] [payload] [CRC],\n"
                                 "COBS-encoded and ended with a 0x00 byte. The DLC is 1 byte.\n"
                                 "The CRC covers [ID] [DLC] [payload] (before COBS).\n"
                                 "The ID and the CRC are sent lowest byte first."));
    frameForm->addRow("ID size:", m_uartIdSizeSpin);
    frameForm->addRow("CRC:", m_uartCrcCombo);

    auto *layout = new QVBoxLayout(tab);
    layout->addWidget(lineGroup);
    layout->addWidget(frameGroup);
    layout->addStretch();
    return tab;
}

QWidget *NetworkDialog::createUdpTab()
{
    auto *tab = new QWidget;

    m_udpModeCombo = new QComboBox;
    for (auto mode : {UdpSettings::Mode::Unicast, UdpSettings::Mode::Broadcast, UdpSettings::Mode::Multicast})
        m_udpModeCombo->addItem(toString(mode));
    connect(m_udpModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NetworkDialog::onUdpModeChanged);

    m_udpLocalAddressEdit = new QLineEdit;
    m_udpLocalPortSpin = createSpin(0, 65535);
    m_udpRemoteAddressEdit = new QLineEdit;
    m_udpRemotePortSpin = createSpin(0, 65535);
    m_udpMulticastGroupEdit = new QLineEdit;
    m_udpIdSizeSpin = createSpin(1, 4, " byte(s)");

    auto *form = new QFormLayout(tab);
    form->addRow("Mode:", m_udpModeCombo);
    form->addRow("Local address:", m_udpLocalAddressEdit);
    form->addRow("Local port:", m_udpLocalPortSpin);
    form->addRow("Remote address:", m_udpRemoteAddressEdit);
    form->addRow("Remote port:", m_udpRemotePortSpin);
    form->addRow("Multicast group:", m_udpMulticastGroupEdit);
    form->addRow("ID size:", m_udpIdSizeSpin);
    form->addRow(new QLabel("Every datagram starts with the message ID, followed by the payload.\n"
                            "The remote port is also used for broadcast and multicast."));
    return tab;
}

void NetworkDialog::onUdpModeChanged()
{
    // The remote address is only used for unicast, the group only for multicast.
    const auto mode = static_cast<UdpSettings::Mode>(m_udpModeCombo->currentIndex());
    m_udpRemoteAddressEdit->setEnabled(mode == UdpSettings::Mode::Unicast);
    m_udpMulticastGroupEdit->setEnabled(mode == UdpSettings::Mode::Multicast);
}
