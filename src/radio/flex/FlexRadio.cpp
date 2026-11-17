// SPDX-License-Identifier: GPL-3.0-or-later
#include "radio/flex/FlexRadio.h"

#include <QRegularExpression>

namespace qrl::flex {

FlexRadio::FlexRadio(QObject *parent)
    : QObject(parent)
{
    connect(&m_connection, &FlexConnection::ready, this, &FlexRadio::onReady);
    connect(&m_connection, &FlexConnection::statusReceived, this, &FlexRadio::onStatus);
    connect(&m_connection, &FlexConnection::disconnected, this, [this] {
        m_transmitDiag.clear();
        m_guiClients.clear();
        m_slices.clear();
        m_boundClientId.clear();
        Q_EMIT guiClientsChanged();
        Q_EMIT slicesChanged();
        Q_EMIT boundClientChanged(QString());
        setState(State::Disconnected);
    });
    connect(&m_connection, &FlexConnection::errorOccurred, this, [this](const QString &msg) {
        Q_EMIT errorOccurred(msg);
        if (!m_connection.isReady())
            setState(State::Disconnected);
    });
    connect(&m_connection, &FlexConnection::commandFailed, this,
            [this](const QString &command, const Message &reply) {
                Q_EMIT logMessage(tr("Befehl fehlgeschlagen: %1 → 0x%2 %3")
                                      .arg(command)
                                      .arg(reply.code, 8, 16, QLatin1Char('0'))
                                      .arg(reply.text));
            });
    connect(&m_connection, &FlexConnection::infoReceived, this, [this](const Message &info) {
        Q_EMIT logMessage(tr("Radio-Meldung: %1").arg(info.text));
    });
}

void FlexRadio::connectToRadio(const QHostAddress &address, quint16 port)
{
    setState(State::Connecting);
    m_connection.connectToRadio(address, port);
}

void FlexRadio::disconnectFromRadio()
{
    m_connection.disconnectFromRadio();
    setState(State::Disconnected);
}

void FlexRadio::bindToClient(const QString &clientId)
{
    m_requestedClientId = clientId;
    if (clientId.isEmpty())
        tryAutoBind();
    else
        sendBind(clientId);
}

const GuiClient *FlexRadio::boundClient() const
{
    for (const GuiClient &client : m_guiClients) {
        if (client.clientId == m_boundClientId)
            return &client;
    }
    return nullptr;
}

void FlexRadio::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    Q_EMIT stateChanged(state);
}

void FlexRadio::onReady()
{
    // Note: "client program <name>" is rejected for non-GUI clients by firmware 4.2
    // (0x10000002 "unknown client program"), so it is not sent.
    m_connection.sendCommand(QStringLiteral("info"), [this](const Message &reply) {
        if (reply.code != 0)
            return;
        const KeyValues info = parseCommaKeyValues(reply.text);
        m_model = info.value(QStringLiteral("model"), m_model);
        m_callsign = info.value(QStringLiteral("callsign"), m_callsign);
        m_nickname = decodeSpaces(info.value(QStringLiteral("name"), m_nickname));
        m_softwareVersion = info.value(QStringLiteral("software_ver"), m_softwareVersion);
        m_numScu = info.value(QStringLiteral("num_scu")).toInt();
        m_numSlices = info.value(QStringLiteral("num_slice")).toInt();
        Q_EMIT logMessage(tr("%1 %2, Firmware %3, %4 SCU, %5 Slices")
                              .arg(m_model, m_callsign, m_softwareVersion)
                              .arg(m_numScu)
                              .arg(m_numSlices));
        Q_EMIT radioInfoChanged();
    });
    for (const char *topic : {"client", "radio", "slice", "tx"})
        m_connection.sendCommand(QStringLiteral("sub %1 all").arg(QLatin1String(topic)));

    setState(State::Connected);
    Q_EMIT logMessage(tr("Verbunden, Handle 0x%1, API-Protokoll %2")
                          .arg(m_connection.handle(), 8, 16, QLatin1Char('0'))
                          .arg(m_connection.version()));
}

void FlexRadio::onStatus(const Message &status)
{
    const QString &object = status.object;

    if (object.startsWith(QLatin1String("client "))) {
        handleClientStatus(status);
    } else if (object.startsWith(QLatin1String("slice "))) {
        bool ok = false;
        const int index = object.mid(6).trimmed().toInt(&ok);
        if (ok)
            handleSliceStatus(index, status.values);
    } else if (object == QLatin1String("radio")) {
        handleRadioStatus(status.values);
    } else if (object == QLatin1String("transmit")) {
        handleTransmitStatus(status.values);
    } else if (object == QLatin1String("interlock")) {
        // Diagnostics: show why the radio does (not) transmit.
        const KeyValues &v = status.values;
        if (v.contains(QStringLiteral("state")) && m_logInterlock) {
            Q_EMIT logMessage(tr("TX-Interlock: %1%2%3")
                                  .arg(v.value(QStringLiteral("state")),
                                       v.value(QStringLiteral("source")).isEmpty() ? QString() : tr(", Quelle %1").arg(v.value(QStringLiteral("source"))),
                                       v.value(QStringLiteral("reason")).isEmpty() ? QString() : tr(", Grund %1").arg(v.value(QStringLiteral("reason")))));
        }
    }
    Q_EMIT statusReceived(status);
}

void FlexRadio::handleTransmitStatus(const KeyValues &v)
{
    bool changed = false;
    if (v.contains(QStringLiteral("speed"))) {
        m_cwSpeed = v.value(QStringLiteral("speed")).toInt();
        changed = true;
    }
    if (v.contains(QStringLiteral("pitch"))) {
        m_cwPitch = v.value(QStringLiteral("pitch")).toInt();
        changed = true;
    }
    if (v.contains(QStringLiteral("break_in"))) {
        m_cwBreakIn = v.value(QStringLiteral("break_in")) == QLatin1String("1");
        changed = true;
    }
    // Diagnostics for CW keying problems: log only values that actually changed
    QStringList diag;
    for (const char *key : {"rfpower", "tunepower", "mox", "synccwx", "break_in", "break_in_delay", "max_power_level", "inhibit", "sidetone", "mon_gain_cw", "pitch"}) {
        const QString k = QLatin1String(key);
        if (v.contains(k) && m_transmitDiag.value(k) != v.value(k)) {
            m_transmitDiag.insert(k, v.value(k));
            diag << QStringLiteral("%1=%2").arg(k, v.value(k));
        }
    }
    if (!diag.isEmpty())
        Q_EMIT logMessage(tr("Transmit: %1").arg(diag.join(QLatin1Char(' '))));
    if (changed)
        Q_EMIT transmitChanged();
}

std::optional<Slice> FlexRadio::txSlice() const
{
    const GuiClient *bound = boundClient();
    std::optional<Slice> fallback;
    for (const Slice &s : m_slices) {
        if (!s.tx)
            continue;
        if (bound && s.clientHandle == bound->handle)
            return s;
        if (!fallback)
            fallback = s;
    }
    return fallback;
}

void FlexRadio::handleClientStatus(const Message &status)
{
    static const QRegularExpression pattern(
        QStringLiteral("^client\\s+0x([0-9a-fA-F]{1,8})(?:\\s+(connected|disconnected))?$"));
    const auto match = pattern.match(status.object);
    if (!match.hasMatch())
        return;

    bool ok = false;
    const quint32 handle = match.captured(1).toUInt(&ok, 16);
    if (!ok || handle == 0 || handle == m_connection.handle())
        return;

    if (match.captured(2) == QLatin1String("disconnected")) {
        const GuiClient removed = m_guiClients.take(handle);
        if (!removed.clientId.isEmpty() && removed.clientId == m_boundClientId) {
            m_boundClientId.clear();
            Q_EMIT boundClientChanged(QString());
        }
        Q_EMIT guiClientsChanged();
        return;
    }

    // Only GUI clients carry a client_id; non-GUI clients (CAT, DAX, loggers) do not.
    GuiClient client = m_guiClients.value(handle);
    client.handle = handle;
    const KeyValues &v = status.values;
    if (v.contains(QStringLiteral("client_id")))
        client.clientId = v.value(QStringLiteral("client_id"));
    if (v.contains(QStringLiteral("program")))
        client.program = decodeSpaces(v.value(QStringLiteral("program")));
    if (v.contains(QStringLiteral("station")))
        client.station = decodeSpaces(v.value(QStringLiteral("station")));

    if (client.clientId.isEmpty()) {
        m_guiClients.remove(handle);
        return;
    }
    m_guiClients.insert(handle, client);
    Q_EMIT guiClientsChanged();
    tryAutoBind();
}

void FlexRadio::handleSliceStatus(int index, const KeyValues &v)
{
    if (v.value(QStringLiteral("in_use")) == QLatin1String("0")) {
        if (m_slices.remove(index) > 0)
            Q_EMIT slicesChanged();
        return;
    }

    Slice &slice = m_slices[index];
    slice.index = index;
    if (v.contains(QStringLiteral("RF_frequency")))
        slice.frequencyMHz = v.value(QStringLiteral("RF_frequency")).toDouble();
    if (v.contains(QStringLiteral("mode")))
        slice.mode = v.value(QStringLiteral("mode"));
    if (v.contains(QStringLiteral("index_letter")))
        slice.letter = v.value(QStringLiteral("index_letter"));
    if (v.contains(QStringLiteral("tx")))
        slice.tx = v.value(QStringLiteral("tx")) == QLatin1String("1");
    if (v.contains(QStringLiteral("active")))
        slice.active = v.value(QStringLiteral("active")) == QLatin1String("1");
    if (v.contains(QStringLiteral("client_handle")))
        slice.clientHandle = v.value(QStringLiteral("client_handle")).toUInt(nullptr, 16);
    Q_EMIT slicesChanged();
}

void FlexRadio::handleRadioStatus(const KeyValues &v)
{
    bool changed = false;
    if (v.contains(QStringLiteral("callsign"))) {
        m_callsign = v.value(QStringLiteral("callsign"));
        changed = true;
    }
    if (v.contains(QStringLiteral("nickname"))) {
        m_nickname = decodeSpaces(v.value(QStringLiteral("nickname")));
        changed = true;
    }
    if (changed)
        Q_EMIT radioInfoChanged();
}

void FlexRadio::tryAutoBind()
{
    if (!m_boundClientId.isEmpty() || m_state != State::Connected)
        return;

    if (!m_requestedClientId.isEmpty()) {
        for (const GuiClient &c : m_guiClients) {
            if (c.clientId == m_requestedClientId) {
                sendBind(c.clientId);
                return;
            }
        }
        return;
    }
    if (!m_preferredStation.isEmpty()) {
        for (const GuiClient &c : m_guiClients) {
            if (c.station == m_preferredStation) {
                sendBind(c.clientId);
                return;
            }
        }
    }
    if (m_guiClients.size() == 1)
        sendBind(m_guiClients.first().clientId);
}

void FlexRadio::sendBind(const QString &clientId)
{
    m_connection.sendCommand(QStringLiteral("client bind client_id=%1").arg(clientId),
                             [this, clientId](const Message &reply) {
                                 if (reply.code != 0) {
                                     Q_EMIT errorOccurred(tr("Bind an %1 fehlgeschlagen (0x%2)")
                                                              .arg(clientId)
                                                              .arg(reply.code, 8, 16, QLatin1Char('0')));
                                     return;
                                 }
                                 m_boundClientId = clientId;
                                 Q_EMIT boundClientChanged(clientId);
                                 const GuiClient *c = boundClient();
                                 Q_EMIT logMessage(tr("Gebunden an %1 (%2)")
                                                       .arg(c ? c->program : QString(),
                                                            c ? c->station : clientId));
                             });
}

} // namespace qrl::flex
