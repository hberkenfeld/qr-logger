// SPDX-License-Identifier: GPL-3.0-or-later
#include "radio/flex/FlexDiscovery.h"

#include "radio/flex/FlexProtocol.h"

#include <QDateTime>
#include <QNetworkDatagram>
#include <QtEndian>

namespace qrl::flex {

namespace {

constexpr qint64 StaleTimeoutMs = 10000;

quint32 readBe32(const QByteArray &data, int offset)
{
    return qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data.constData()) + offset);
}

QStringList splitList(const QString &value)
{
    QStringList out;
    const auto items = value.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &item : items) {
        const QString trimmed = decodeSpaces(item);
        if (!trimmed.isEmpty())
            out << trimmed;
    }
    return out;
}

} // namespace

QString RadioInfo::displayName() const
{
    QString name = nickname.isEmpty() ? model : QStringLiteral("%1 (%2)").arg(nickname, model);
    if (!callsign.isEmpty())
        name += QStringLiteral(" – %1").arg(callsign);
    return QStringLiteral("%1 @ %2").arg(name, address.toString());
}

FlexDiscovery::FlexDiscovery(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &FlexDiscovery::onReadyRead);
    m_staleTimer.setInterval(2000);
    connect(&m_staleTimer, &QTimer::timeout, this, &FlexDiscovery::onStaleCheck);
}

bool FlexDiscovery::start(quint16 port)
{
    if (m_socket.state() == QAbstractSocket::BoundState)
        return true;
    // Other Flex clients on this host listen on the same port.
    const bool ok = m_socket.bind(QHostAddress::AnyIPv4, port,
                                  QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (ok)
        m_staleTimer.start();
    return ok;
}

void FlexDiscovery::stop()
{
    m_staleTimer.stop();
    m_socket.close();
}

std::optional<QByteArray> FlexDiscovery::extractPayload(const QByteArray &datagram)
{
    if (datagram.size() < 16)
        return std::nullopt;

    const quint32 header = readBe32(datagram, 0);
    const quint32 packetType = header >> 28;       // 0x3 = extension data with stream id
    const bool classIdPresent = header & (1u << 27);
    const quint32 tsi = (header >> 22) & 0x3;
    const quint32 tsf = (header >> 20) & 0x3;
    const int sizeBytes = int(header & 0xFFFF) * 4;

    if (packetType != 0x3 || !classIdPresent)
        return std::nullopt;
    if (readBe32(datagram, 4) != StreamId || readBe32(datagram, 12) != ClassIdLow)
        return std::nullopt;

    const int headerBytes = 16 + (tsi ? 4 : 0) + (tsf ? 8 : 0);
    const int end = qMin(int(datagram.size()), sizeBytes > 0 ? sizeBytes : int(datagram.size()));
    if (end <= headerBytes)
        return std::nullopt;

    QByteArray payload = datagram.mid(headerBytes, end - headerBytes);
    const int nul = payload.indexOf('\0');
    if (nul >= 0)
        payload.truncate(nul);
    return payload;
}

RadioInfo FlexDiscovery::parsePayload(const QByteArray &payload)
{
    RadioInfo info;
    const KeyValues kv = parseKeyValues(QString::fromUtf8(payload).trimmed());

    info.serial = kv.value(QStringLiteral("serial"));
    info.model = kv.value(QStringLiteral("model"));
    info.nickname = decodeSpaces(kv.value(QStringLiteral("nickname")));
    info.callsign = kv.value(QStringLiteral("callsign"));
    info.version = kv.value(QStringLiteral("version"));
    info.status = kv.value(QStringLiteral("status"));
    info.address = QHostAddress(kv.value(QStringLiteral("ip")));
    const quint16 port = kv.value(QStringLiteral("port")).toUShort();
    if (port != 0)
        info.port = port;
    info.guiClientPrograms = splitList(kv.value(QStringLiteral("gui_client_programs")));
    info.guiClientStations = splitList(kv.value(QStringLiteral("gui_client_stations")));
    info.guiClientHandles = splitList(kv.value(QStringLiteral("gui_client_handles")));
    return info;
}

void FlexDiscovery::onReadyRead()
{
    while (m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket.receiveDatagram();
        const auto payload = extractPayload(datagram.data());
        if (!payload)
            continue;

        RadioInfo info = parsePayload(*payload);
        if (info.serial.isEmpty())
            continue;
        if (info.address.isNull())
            info.address = datagram.senderAddress();
        info.lastSeenMs = QDateTime::currentMSecsSinceEpoch();

        bool known = false;
        for (RadioInfo &existing : m_radios) {
            if (existing.serial == info.serial) {
                existing = info;
                known = true;
                break;
            }
        }
        if (known) {
            Q_EMIT radioUpdated(info);
        } else {
            m_radios.append(info);
            Q_EMIT radioFound(info);
        }
    }
}

void FlexDiscovery::onStaleCheck()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (int i = m_radios.size() - 1; i >= 0; --i) {
        if (now - m_radios.at(i).lastSeenMs > StaleTimeoutMs) {
            const QString serial = m_radios.at(i).serial;
            m_radios.removeAt(i);
            Q_EMIT radioLost(serial);
        }
    }
}

} // namespace qrl::flex
