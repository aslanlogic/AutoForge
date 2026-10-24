#include "udpbus.h"

#include <QNetworkDatagram>
#include <QUdpSocket>

UdpBus::UdpBus(QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
{
    connect(m_socket, &QUdpSocket::readyRead, this, &UdpBus::readPendingDatagrams);
}

bool UdpBus::open(const QHostAddress &bindAddress, quint16 bindPort, const QHostAddress &remoteAddress,
                  quint16 remotePort, int idSize, QString *errorMessage)
{
    close();
    if (!m_socket->bind(bindAddress, bindPort)) {
        *errorMessage = QString("Could not listen on %1:%2: %3")
                            .arg(bindAddress.toString()).arg(bindPort).arg(m_socket->errorString());
        return false;
    }
    m_remoteAddress = remoteAddress;
    m_remotePort = remotePort;
    m_idSize = qBound(1, idSize, 4);
    return true;
}

void UdpBus::close()
{
    m_socket->close();
}

bool UdpBus::isOpen() const
{
    return m_socket->state() == QAbstractSocket::BoundState;
}

void UdpBus::send(quint32 id, const QByteArray &payload)
{
    if (!isOpen())
        return;
    QByteArray datagram;
    for (int i = 0; i < m_idSize; ++i)
        datagram.append(char((id >> (8 * i)) & 0xFF));
    datagram.append(payload);
    m_socket->writeDatagram(datagram, m_remoteAddress, m_remotePort);
}

void UdpBus::readPendingDatagrams()
{
    while (m_socket->hasPendingDatagrams()) {
        const QByteArray datagram = m_socket->receiveDatagram().data();
        if (datagram.size() < m_idSize)
            continue; // not one of ours
        quint32 id = 0;
        for (int i = 0; i < m_idSize; ++i)
            id |= quint32(quint8(datagram[i])) << (8 * i);
        emit received(id, datagram.mid(m_idSize));
    }
}
