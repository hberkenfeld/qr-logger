// SPDX-License-Identifier: GPL-3.0-or-later
#include "tx/CwKeyer.h"

#include "radio/flex/FlexRadio.h"
#include "tx/Macros.h"

namespace qrl {

CwKeyer::CwKeyer(flex::FlexRadio *radio, QObject *parent)
    : QObject(parent)
    , m_radio(radio)
{
    m_fallback.setSingleShot(true);
    connect(&m_fallback, &QTimer::timeout, this, [this] {
        if (!m_statusSeen)
            Q_EMIT diagnostic(tr("CWX: keine 'cwx sent'-Statusmeldung empfangen – Ende per Zeitschätzung"));
        finishMessage();
    });
    connect(m_radio, &flex::FlexRadio::statusReceived, this, &CwKeyer::onStatus);
    connect(m_radio, &flex::FlexRadio::transmitChanged, this, [this] {
        const int radioSpeed = m_radio->cwSpeed();
        if (radioSpeed > 0 && radioSpeed != m_speed) {
            m_speed = radioSpeed;
            Q_EMIT speedChanged(m_speed);
        }
    });
}

QString CwKeyer::encodeText(const QString &text)
{
    QString out = text.toUpper();
    out.replace(QLatin1Char(' '), QChar(0x7f));
    out.replace(QLatin1Char('"'), QChar(0x1e));
    return out;
}

void CwKeyer::send(const QString &text)
{
    const QString clean = text.simplified().toUpper();
    if (clean.isEmpty() || m_radio->state() != flex::FlexRadio::State::Connected)
        return;

    if (m_holdPtt && !m_pttKeyed) {
        m_radio->connection().sendCommand(QStringLiteral("xmit 1"));
        m_pttKeyed = true;
    }
    const int block = ++m_block;
    const int chars = int(clean.size());
    m_radio->connection().sendCommand(
        QStringLiteral("cwx send \"%1\" %2").arg(encodeText(clean)).arg(block),
        [this, block, chars](const flex::Message &reply) {
            if (reply.code != 0) {
                Q_EMIT diagnostic(tr("cwx send abgelehnt: 0x%1 %2").arg(reply.code, 8, 16, QLatin1Char('0')).arg(reply.text));
                return;
            }
            const QStringList parts = reply.text.split(QLatin1Char(','));
            bool ok = false;
            const int index = parts.value(0).trimmed().toInt(&ok);
            if (ok && parts.value(1).trimmed().toInt() == block)
                m_endIndex = qMax(m_endIndex, index + chars - 1);
        });

    // Fallback: estimated duration of everything queued, plus margin.
    const int remaining = m_fallback.isActive() ? m_fallback.remainingTime() : 0;
    m_fallback.start(remaining + morseDurationMs(clean + QLatin1Char(' '), m_speed) + 300);
    m_statusSeen = false;
    setSending(true);
    Q_EMIT sent(clean);
}

void CwKeyer::stop()
{
    if (m_radio->state() == flex::FlexRadio::State::Connected)
        m_radio->connection().sendCommand(QStringLiteral("cwx clear"));
    m_fallback.stop();
    m_endIndex = -1;
    if (m_pttKeyed) {
        m_radio->connection().sendCommand(QStringLiteral("xmit 0"));
        m_pttKeyed = false;
    }
    setSending(false);
}

void CwKeyer::finishMessage()
{
    m_endIndex = -1;
    m_fallback.stop();
    if (m_pttKeyed) {
        m_radio->connection().sendCommand(QStringLiteral("xmit 0"));
        m_pttKeyed = false;
    }
    setSending(false);
    Q_EMIT finished();
}

void CwKeyer::setSpeed(int wpm)
{
    wpm = qBound(10, wpm, 60);
    if (wpm == m_speed)
        return;
    m_speed = wpm;
    if (m_radio->state() == flex::FlexRadio::State::Connected)
        m_radio->connection().sendCommand(QStringLiteral("cwx wpm %1").arg(wpm));
    Q_EMIT speedChanged(m_speed);
}

void CwKeyer::onStatus(const flex::Message &status)
{
    if (status.object != QLatin1String("cwx"))
        return;
    if (status.values.contains(QStringLiteral("wpm"))) {
        const int wpm = status.values.value(QStringLiteral("wpm")).toInt();
        if (wpm > 0 && wpm != m_speed) {
            m_speed = wpm;
            Q_EMIT speedChanged(m_speed);
        }
    }
    if (status.values.contains(QStringLiteral("sent"))) {
        if (!m_statusSeen && m_sending) {
            m_statusSeen = true;
            Q_EMIT diagnostic(tr("CWX-Status empfangen (sent=%1, Ende bei %2)")
                                  .arg(status.values.value(QStringLiteral("sent")))
                                  .arg(m_endIndex));
        }
        const int sentIndex = status.values.value(QStringLiteral("sent")).toInt();
        if (m_endIndex >= 0 && sentIndex >= m_endIndex)
            finishMessage();
    }
}

void CwKeyer::setSending(bool sending)
{
    if (m_sending == sending)
        return;
    m_sending = sending;
    Q_EMIT sendingChanged(sending);
}

} // namespace qrl
