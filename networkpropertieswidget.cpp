#include "networkpropertieswidget.h"
#include "database.h"
#include "propertytable.h"

#include <QLocale>
#include <QVBoxLayout>

namespace {

QString number(double value)
{
    return QLocale::c().toString(value, 'g', QLocale::FloatingPointShortest);
}

// The settings of the active protocol, one row each.
QVector<QPair<QString, QString>> protocolRows(const Network &network)
{
    switch (network.protocol) {
    case Network::Protocol::Can:
        return {
            {"Bitrate", QString("%1 bit/s").arg(network.can.bitrate)},
            {"Sample point", number(network.can.samplePoint) + " %"},
        };
    case Network::Protocol::CanFd:
        return {
            {"Nominal bitrate", QString("%1 bit/s").arg(network.canFd.nominalBitrate)},
            {"Nominal sample point", number(network.canFd.nominalSamplePoint) + " %"},
            {"Data bitrate", QString("%1 bit/s").arg(network.canFd.dataBitrate)},
            {"Data sample point", number(network.canFd.dataSamplePoint) + " %"},
            {"Variant", network.canFd.iso ? "ISO CAN FD" : "non-ISO CAN FD"},
        };
    case Network::Protocol::Uart:
        return {
            {"Baud rate", QString::number(network.uart.baudRate)},
            {"Data bits", QString::number(network.uart.dataBits)},
            {"Parity", toString(network.uart.parity)},
            {"Stop bits", toString(network.uart.stopBits)},
            {"Flow control", toString(network.uart.flowControl)},
            {"Start bytes", bytesToHexText(network.uart.startBytes)},
            {"ID size", QString("%1 byte(s)").arg(network.uart.idSize)},
            {"Checksum", toString(network.uart.checksum)},
        };
    case Network::Protocol::Udp: {
        QVector<QPair<QString, QString>> rows = {
            {"Mode", toString(network.udp.mode)},
            {"Local address", QString("%1:%2").arg(network.udp.localAddress).arg(network.udp.localPort)},
        };
        if (network.udp.mode == UdpSettings::Mode::Unicast)
            rows.append({"Remote address", QString("%1:%2").arg(network.udp.remoteAddress).arg(network.udp.remotePort)});
        else if (network.udp.mode == UdpSettings::Mode::Multicast)
            rows.append({"Multicast group", QString("%1:%2").arg(network.udp.multicastGroup).arg(network.udp.remotePort)});
        else
            rows.append({"Broadcast port", QString::number(network.udp.remotePort)});
        rows.append({"ID size", QString("%1 byte(s)").arg(network.udp.idSize)});
        return rows;
    }
    }
    return {};
}

} // namespace

NetworkPropertiesWidget::NetworkPropertiesWidget(QWidget *parent)
    : QWidget(parent)
{
    m_propertiesTable = createReadOnlyTable({"Property", "Value"});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_propertiesTable);
    layout->addStretch();
}

void NetworkPropertiesWidget::showNetwork(const Database &database, const QString &filePath)
{
    QVector<QPair<QString, QString>> rows = {
        {"File", filePath.isEmpty() ? "(not saved yet)" : filePath},
        {"Network Nodes", QString::number(database.allNodes().size())},
        {"Messages", QString::number(database.allMessages().size())},
        {"Signals", QString::number(database.allSignals().size())},
        {"Protocol", toString(database.network().protocol)},
    };
    rows += protocolRows(database.network());
    setPropertyRows(m_propertiesTable, rows);
}
