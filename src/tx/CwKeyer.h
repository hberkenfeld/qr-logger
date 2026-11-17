// SPDX-License-Identifier: GPL-3.0-or-later
//
// CW sending via the radio's CWX buffer (SmartSDR API):
//   cwx send "<text>" <block>  – spaces as 0x7F, quotes as 0x1E; reply "<index>,<block>"
//   cwx wpm <n>, cwx clear
//   status "cwx sent=<index>" reports progress
// The radio keys the transmitter itself when break-in is enabled.
// Encoding as in FlexLib CWX.cs (cross-checked with AetherSDR, GPL-3).
#pragma once

#include <QObject>
#include <QTimer>

namespace qrl {

namespace flex {
class FlexRadio;
struct Message;
}

class CwKeyer : public QObject
{
    Q_OBJECT

public:
    explicit CwKeyer(flex::FlexRadio *radio, QObject *parent = nullptr);

    void send(const QString &text);
    void stop();
    void setSpeed(int wpm);
    void changeSpeed(int delta) { setSpeed(m_speed + delta); }
    int speed() const { return m_speed; }
    bool isSending() const { return m_sending; }

    // Hold PTT ("xmit 1" … "xmit 0") for the whole message instead of relying on break-in.
    void setHoldPtt(bool hold) { m_holdPtt = hold; }
    bool holdPtt() const { return m_holdPtt; }

    static QString encodeText(const QString &text);

Q_SIGNALS:
    void sendingChanged(bool sending);
    void speedChanged(int wpm);
    void finished();
    void sent(const QString &text);
    void diagnostic(const QString &text);
    void stopped();

private:
    void onStatus(const flex::Message &status);
    void setSending(bool sending);
    void finishMessage();

    flex::FlexRadio *m_radio;
    QTimer m_fallback;      // ends "sending" if no cwx status arrives
    int m_speed = 28;
    int m_block = 0;
    int m_endIndex = -1;
    bool m_sending = false;
    bool m_holdPtt = false;
    bool m_pttKeyed = false;
    bool m_statusSeen = false;
};

} // namespace qrl
