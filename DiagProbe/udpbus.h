#ifndef UDPBUS_H
#define UDPBUS_H

#include <QHostAddress>
#include <QObject>

class QUdpSocket;

// A UDP "bus": every datagram is one message,
//     [message ID, idSize bytes, little-endian (lowest byte first)] [payload]
// the same ID order as the UART framing of FrameForge and UDScribe.
class UdpBus : public QObject
{
    Q_OBJECT

public:
    explicit UdpBus(QObject *parent = nullptr);

    // Listens on bindAddress:bindPort and sends to remoteAddress:remotePort.
    bool open(const QHostAddress &bindAddress, quint16 bindPort, const QHostAddress &remoteAddress,
              quint16 remotePort, int idSize, QString *errorMessage);
    void close();
    bool isOpen() const;

    void send(quint32 id, const QByteArray &payload);

signals:
    void received(quint32 id, const QByteArray &payload);

private:
    void readPendingDatagrams();

    QUdpSocket *m_socket;
    QHostAddress m_remoteAddress;
    quint16 m_remotePort = 0;
    int m_idSize = 4;
};

#endif // UDPBUS_H
