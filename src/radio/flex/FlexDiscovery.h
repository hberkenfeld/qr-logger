// SPDX-License-Identifier: GPL-3.0-or-later
//
// Radio discovery: the radio broadcasts a VITA-49 extension data packet
// (stream id 0x800, class id 0x534CFFFF) to UDP port 4992 about once per second.
// The payload is a list of key=value pairs separated by spaces.
#pragma once

#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUdpSocket>

#include <optional>

namespace qrl::flex {

struct RadioInfo
{
    QString serial;
    QString model;
    QString nickname;
    QString callsign;
    QString version;
    QString status;
    QHostAddress address;
    quint16 port = 4992;
    QStringList guiClientPrograms;
    QStringList guiClientStations;
    QStringList guiClientHandles;
    qint64 lastSeenMs = 0;

    QString displayName() const;
};

class FlexDiscovery : public QObject
{
    Q_OBJECT

public:
    static constexpr quint16 Port = 4992;
    static constexpr quint32 StreamId = 0x800;
    static constexpr quint32 ClassIdLow = 0x534CFFFF;

    explicit FlexDiscovery(QObject *parent = nullptr);

    bool start(quint16 port = Port);
    void stop();
    QList<RadioInfo> radios() const { return m_radios; }

    // Returns the payload of a discovery packet, or nothing if the datagram is not one.
    static std::optional<QByteArray> extractPayload(const QByteArray &datagram);
    static RadioInfo parsePayload(const QByteArray &payload);

Q_SIGNALS:
    void radioFound(const qrl::flex::RadioInfo &info);
    void radioUpdated(const qrl::flex::RadioInfo &info);
    void radioLost(const QString &serial);

private:
    void onReadyRead();
    void onStaleCheck();

    QUdpSocket m_socket;
    QTimer m_staleTimer;
    QList<RadioInfo> m_radios;
};

} // namespace qrl::flex
