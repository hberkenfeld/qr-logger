// SPDX-License-Identifier: GPL-3.0-or-later
//
// State model of one FlexRadio as seen by QR-Logger.
//
// QR-Logger runs as a non-GUI client next to a GUI client (AetherSDR, Maestro,
// SmartSDR) and binds to it with "client bind client_id=<id>".
#pragma once

#include "radio/flex/FlexConnection.h"

#include <QHostAddress>
#include <QList>
#include <QMap>
#include <QObject>

namespace qrl::flex {

struct Slice
{
    int index = -1;
    QString letter;          // "A", "B", ...
    double frequencyMHz = 0.0;
    QString mode;
    bool tx = false;
    bool active = false;
    quint32 clientHandle = 0; // GUI client owning the slice
};

struct GuiClient
{
    quint32 handle = 0;
    QString clientId;        // GUID used for "client bind"
    QString program;
    QString station;
};

class FlexRadio : public QObject
{
    Q_OBJECT

public:
    enum class State { Disconnected, Connecting, Connected };
    Q_ENUM(State)

    explicit FlexRadio(QObject *parent = nullptr);

    void connectToRadio(const QHostAddress &address, quint16 port = 4992);
    void disconnectFromRadio();

    // Binds to the given GUI client. With an empty id the radio binds
    // automatically as soon as exactly one GUI client is present.
    void bindToClient(const QString &clientId);
    void setPreferredStation(const QString &station) { m_preferredStation = station; }

    State state() const { return m_state; }
    QString version() const { return m_connection.version(); }
    quint32 handle() const { return m_connection.handle(); }
    QString model() const { return m_model; }
    QString callsign() const { return m_callsign; }
    QString nickname() const { return m_nickname; }
    QString softwareVersion() const { return m_softwareVersion; }
    int numScu() const { return m_numScu; }
    int numSlices() const { return m_numSlices; }

    QList<GuiClient> guiClients() const { return m_guiClients.values(); }
    QString boundClientId() const { return m_boundClientId; }
    const GuiClient *boundClient() const;

    QList<Slice> slices() const { return m_slices.values(); }

    FlexConnection &connection() { return m_connection; }

Q_SIGNALS:
    void stateChanged(qrl::flex::FlexRadio::State state);
    void radioInfoChanged();
    void guiClientsChanged();
    void boundClientChanged(const QString &clientId);
    void slicesChanged();
    void errorOccurred(const QString &message);
    void logMessage(const QString &message);

private:
    void setState(State state);
    void onReady();
    void onStatus(const Message &status);
    void handleClientStatus(const Message &status);
    void handleSliceStatus(int index, const KeyValues &values);
    void handleRadioStatus(const KeyValues &values);
    void tryAutoBind();
    void sendBind(const QString &clientId);

    FlexConnection m_connection;
    State m_state = State::Disconnected;
    QString m_model;
    QString m_callsign;
    QString m_nickname;
    QString m_softwareVersion;
    int m_numScu = 0;
    int m_numSlices = 0;
    QMap<quint32, GuiClient> m_guiClients;
    QString m_boundClientId;
    QString m_requestedClientId;
    QString m_preferredStation;
    QMap<int, Slice> m_slices;
};

} // namespace qrl::flex
