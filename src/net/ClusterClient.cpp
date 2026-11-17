// SPDX-License-Identifier: GPL-3.0-or-later
#include "net/ClusterClient.h"

#include <QRegularExpression>

namespace qrl {

std::optional<Spot> parseSpotLine(const QString &line)
{
    static const QRegularExpression spotRe(
        QStringLiteral("^DX\\s+de\\s+(\\S+?):?\\s+(\\d+(?:\\.\\d+)?)\\s+(\\S+)\\s*(.*?)\\s*(\\d{4})Z"),
        QRegularExpression::CaseInsensitiveOption);
    const auto m = spotRe.match(line.trimmed());
    if (!m.hasMatch())
        return std::nullopt;

    Spot spot;
    spot.spotter = m.captured(1).toUpper();
    if (spot.spotter.endsWith(QLatin1Char(':')))
        spot.spotter.chop(1);
    spot.freqKhz = m.captured(2).toDouble();
    spot.call = m.captured(3).toUpper();
    spot.comment = m.captured(4).simplified();
    spot.time = QDateTime::currentDateTimeUtc();
    spot.skimmer = spot.spotter.endsWith(QLatin1String("-#"));
    if (spot.freqKhz <= 0 || spot.call.isEmpty())
        return std::nullopt;

    static const QRegularExpression wpmRe(QStringLiteral("\\b(\\d{1,2})\\s*WPM\\b"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression snrRe(QStringLiteral("([+-]?\\d{1,2})\\s*dB\\b"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression modeRe(QStringLiteral("\\b(CW|RTTY|FT8|FT4|PSK31|PSK|SSB|USB|LSB)\\b"), QRegularExpression::CaseInsensitiveOption);
    if (const auto w = wpmRe.match(spot.comment); w.hasMatch())
        spot.wpm = w.captured(1).toInt();
    if (const auto d = snrRe.match(spot.comment); d.hasMatch())
        spot.snr = d.captured(1).toInt();
    if (const auto md = modeRe.match(spot.comment); md.hasMatch())
        spot.mode = md.captured(1).toUpper();
    return spot;
}

ClusterClient::ClusterClient(const QString &name, QObject *parent)
    : QObject(parent)
    , m_name(name)
{
    connect(&m_socket, &QTcpSocket::connected, this, [this] {
        m_loggedIn = false;
        setState(State::Connected);
    });
    connect(&m_socket, &QTcpSocket::readyRead, this, &ClusterClient::onReadyRead);
    connect(&m_socket, &QTcpSocket::disconnected, this, [this] {
        setState(State::Disconnected);
        if (m_wanted)
            m_reconnect.start();
    });
    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        Q_EMIT errorOccurred(m_socket.errorString());
        if (m_socket.state() == QAbstractSocket::UnconnectedState) {
            setState(State::Disconnected);
            if (m_wanted)
                m_reconnect.start();
        }
    });
    m_reconnect.setSingleShot(true);
    m_reconnect.setInterval(15000);
    connect(&m_reconnect, &QTimer::timeout, this, [this] {
        if (m_wanted && m_state == State::Disconnected) {
            setState(State::Connecting);
            m_socket.connectToHost(m_host, m_port);
        }
    });
}

void ClusterClient::connectTo(const QString &host, quint16 port, const QString &callsign)
{
    m_wanted = false;
    m_socket.abort();
    m_host = host;
    m_port = port;
    m_callsign = callsign.toUpper();
    m_buffer.clear();
    m_wanted = true;
    setState(State::Connecting);
    m_socket.connectToHost(host, port);
}

void ClusterClient::disconnectFrom()
{
    m_wanted = false;
    m_reconnect.stop();
    if (m_socket.state() == QAbstractSocket::ConnectedState)
        m_socket.write("bye\r\n");
    m_socket.disconnectFromHost();
    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.abort();
    setState(State::Disconnected);
}

void ClusterClient::sendCommand(const QString &command)
{
    if (m_state == State::Connected)
        m_socket.write((command + QStringLiteral("\r\n")).toLatin1());
}

void ClusterClient::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    Q_EMIT stateChanged(state);
}

bool ClusterClient::isLoginPrompt(const QString &text) const
{
    const QString lower = text.trimmed().toLower();
    return lower.endsWith(QLatin1String("login:")) || lower.endsWith(QLatin1String("call:"))
        || lower.endsWith(QLatin1String("callsign:")) || lower.contains(QLatin1String("enter your call"));
}

void ClusterClient::onReadyRead()
{
    QByteArray data = m_socket.readAll();
    // Strip telnet IAC sequences (0xFF cmd opt)
    for (int i = 0; i < data.size();) {
        if (uchar(data.at(i)) == 0xFF && i + 2 < data.size())
            data.remove(i, 3);
        else
            ++i;
    }
    m_buffer.append(data);
    if (m_buffer.size() > 1 << 20)
        m_buffer.clear();

    int nl;
    while ((nl = m_buffer.indexOf('\n')) >= 0) {
        const QString line = QString::fromLatin1(m_buffer.left(nl)).remove(QLatin1Char('\r')).remove(QChar(0x07));
        m_buffer.remove(0, nl + 1);
        handleLine(line);
    }
    // Login prompts usually come without newline
    if (!m_loggedIn && !m_buffer.isEmpty() && isLoginPrompt(QString::fromLatin1(m_buffer))) {
        m_buffer.clear();
        m_socket.write((m_callsign + QStringLiteral("\r\n")).toLatin1());
        m_loggedIn = true;
    }
}

void ClusterClient::handleLine(const QString &line)
{
    if (!m_loggedIn && isLoginPrompt(line)) {
        m_socket.write((m_callsign + QStringLiteral("\r\n")).toLatin1());
        m_loggedIn = true;
        return;
    }
    if (auto spot = parseSpotLine(line)) {
        spot->source = m_name;
        Q_EMIT spotReceived(*spot);
        return;
    }
    if (!line.trimmed().isEmpty())
        Q_EMIT lineReceived(line);
}

} // namespace qrl
