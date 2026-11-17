// SPDX-License-Identifier: GPL-3.0-or-later
//
// Telnet client for DX clusters and skimmer servers (RBN, CW Skimmer Server).
// Spot format: "DX de <spotter>: <kHz> <call> <comment> <hhmm>Z"
#pragma once

#include <QDateTime>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

#include <optional>

namespace qrl {

struct Spot
{
    QString call;
    double freqKhz = 0.0;
    QString spotter;
    QString comment;
    QDateTime time;      // UTC, time of reception
    int wpm = 0;         // skimmer speed, 0 = unknown
    int snr = 0;         // skimmer dB, 0 = unknown
    QString mode;        // "CW", "RTTY", … if stated in the comment
    bool skimmer = false;
    QString source;      // connection name
};

std::optional<Spot> parseSpotLine(const QString &line);

class ClusterClient : public QObject
{
    Q_OBJECT

public:
    enum class State { Disconnected, Connecting, Connected };
    Q_ENUM(State)

    explicit ClusterClient(const QString &name, QObject *parent = nullptr);

    QString name() const { return m_name; }
    State state() const { return m_state; }
    QString endpoint() const { return QStringLiteral("%1:%2").arg(m_host).arg(m_port); }

    void connectTo(const QString &host, quint16 port, const QString &callsign);
    void disconnectFrom();
    void sendCommand(const QString &command);

Q_SIGNALS:
    void stateChanged(qrl::ClusterClient::State state);
    void spotReceived(const qrl::Spot &spot);
    void lineReceived(const QString &line);
    void errorOccurred(const QString &message);

private:
    void onReadyRead();
    void setState(State state);
    void handleLine(const QString &line);
    bool isLoginPrompt(const QString &text) const;

    QString m_name;
    QTcpSocket m_socket;
    QTimer m_reconnect;
    QByteArray m_buffer;
    QString m_host;
    quint16 m_port = 0;
    QString m_callsign;
    State m_state = State::Disconnected;
    bool m_loggedIn = false;
    bool m_wanted = false;
};

} // namespace qrl
