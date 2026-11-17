// SPDX-License-Identifier: GPL-3.0-or-later
#include "radio/flex/FlexConnection.h"

namespace qrl::flex {

FlexConnection::FlexConnection(QObject *parent)
    : QObject(parent)
{
    m_socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);
    connect(&m_socket, &QTcpSocket::readyRead, this, &FlexConnection::onReadyRead);
    connect(&m_socket, &QTcpSocket::disconnected, this, &FlexConnection::onDisconnected);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        Q_EMIT errorOccurred(m_socket.errorString());
        if (m_socket.state() != QAbstractSocket::ConnectedState)
            onDisconnected();
    });

    m_pingTimer.setInterval(1000);
    connect(&m_pingTimer, &QTimer::timeout, this, [this] { sendCommand(QStringLiteral("ping")); });
}

void FlexConnection::connectToRadio(const QHostAddress &address, quint16 port)
{
    disconnectFromRadio();
    m_socket.connectToHost(address, port);
}

void FlexConnection::disconnectFromRadio()
{
    if (m_socket.state() == QAbstractSocket::UnconnectedState)
        return;
    m_socket.disconnectFromHost();
    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.abort();
    onDisconnected();
}

quint32 FlexConnection::sendCommand(const QString &command, ReplyHandler handler)
{
    if (!isReady())
        return 0;
    const quint32 seq = m_nextSequence++;
    m_pending.insert(seq, Pending{command, std::move(handler)});
    m_socket.write(buildCommand(seq, command));
    return seq;
}

void FlexConnection::onReadyRead()
{
    while (m_socket.canReadLine()) {
        const QByteArray raw = m_socket.readLine();
        QString line = QString::fromUtf8(raw);
        while (line.endsWith(QLatin1Char('\n')) || line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        if (!line.isEmpty())
            handleLine(line);
    }
}

void FlexConnection::onDisconnected()
{
    const bool wasReady = m_handle != 0;
    m_pingTimer.stop();
    m_pending.clear();
    m_handle = 0;
    m_version.clear();
    m_nextSequence = 1;
    if (wasReady)
        Q_EMIT disconnected();
}

void FlexConnection::handleLine(const QString &line)
{
    const Message msg = parseLine(line);
    switch (msg.type) {
    case Message::Type::Version:
        m_version = msg.text;
        break;
    case Message::Type::Handle:
        m_handle = msg.handle;
        sendCommand(QStringLiteral("keepalive enable"));
        m_pingTimer.start();
        Q_EMIT ready();
        break;
    case Message::Type::Reply: {
        const Pending pending = m_pending.take(msg.sequence);
        if (msg.code != 0 && pending.command != QLatin1String("ping"))
            Q_EMIT commandFailed(pending.command, msg);
        if (pending.handler)
            pending.handler(msg);
        break;
    }
    case Message::Type::Status:
        Q_EMIT statusReceived(msg);
        break;
    case Message::Type::Info:
        Q_EMIT infoReceived(msg);
        break;
    case Message::Type::Unknown:
        break;
    }
}

} // namespace qrl::flex
