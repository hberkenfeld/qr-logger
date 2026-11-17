// SPDX-License-Identifier: GPL-3.0-or-later
//
// TCP command channel to a FlexRadio (port 4992): sequencing of commands,
// dispatch of replies, status and info messages, keepalive.
#pragma once

#include "radio/flex/FlexProtocol.h"

#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

#include <functional>

namespace qrl::flex {

class FlexConnection : public QObject
{
    Q_OBJECT

public:
    using ReplyHandler = std::function<void(const Message &reply)>;

    explicit FlexConnection(QObject *parent = nullptr);

    void connectToRadio(const QHostAddress &address, quint16 port = 4992);
    void disconnectFromRadio();

    // Returns the sequence number, or 0 if not connected.
    quint32 sendCommand(const QString &command, ReplyHandler handler = {});

    bool isReady() const { return m_handle != 0; }
    quint32 handle() const { return m_handle; }
    QString version() const { return m_version; }
    QHostAddress peerAddress() const { return m_socket.peerAddress(); }

Q_SIGNALS:
    void ready();                       // handle received, commands may be sent
    void disconnected();
    void errorOccurred(const QString &message);
    void statusReceived(const qrl::flex::Message &status);
    void infoReceived(const qrl::flex::Message &info);
    void commandFailed(const QString &command, const qrl::flex::Message &reply);

private:
    void onReadyRead();
    void onDisconnected();
    void handleLine(const QString &line);

    struct Pending
    {
        QString command;
        ReplyHandler handler;
    };

    QTcpSocket m_socket;
    QTimer m_pingTimer;
    QHash<quint32, Pending> m_pending;
    quint32 m_nextSequence = 1;
    quint32 m_handle = 0;
    QString m_version;
};

} // namespace qrl::flex
