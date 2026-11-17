// SPDX-License-Identifier: GPL-3.0-or-later
//
// qrl-flexprobe – console test tool for the FlexRadio connection.
// Lists radios found via discovery, connects, binds to a GUI client and prints
// GUI clients and slices.

#include "radio/flex/FlexDiscovery.h"
#include "radio/flex/FlexRadio.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

using namespace qrl::flex;

namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

void printState(const FlexRadio &radio)
{
    out() << "\n--- " << radio.model() << " " << radio.nickname() << " " << radio.callsign() << " ---\n";
    out() << "GUI-Clients:\n";
    for (const GuiClient &c : radio.guiClients()) {
        out() << QStringLiteral("  0x%1  %2  %3  %4%5\n")
                     .arg(c.handle, 8, 16, QLatin1Char('0'))
                     .arg(c.program, c.station, c.clientId,
                          c.clientId == radio.boundClientId() ? QStringLiteral("  [gebunden]") : QString());
    }
    out() << "Slices:\n";
    for (const Slice &s : radio.slices()) {
        out() << QStringLiteral("  %1 (%2)  %3 MHz  %4%5%6  Client 0x%7\n")
                     .arg(s.index)
                     .arg(s.letter)
                     .arg(s.frequencyMHz, 0, 'f', 6)
                     .arg(s.mode, s.tx ? QStringLiteral("  TX") : QString(),
                          s.active ? QStringLiteral("  aktiv") : QString())
                     .arg(s.clientHandle, 8, 16, QLatin1Char('0'));
    }
    out().flush();
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qrl-flexprobe"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QRL_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("QR-Logger FlexRadio test tool"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption ipOpt(QStringLiteral("ip"), QStringLiteral("Radio IP address (otherwise first discovered radio)."), QStringLiteral("address"));
    QCommandLineOption portOpt(QStringLiteral("port"), QStringLiteral("TCP port (default 4992)."), QStringLiteral("port"), QStringLiteral("4992"));
    QCommandLineOption bindOpt(QStringLiteral("bind"), QStringLiteral("client_id of the GUI client (default: automatic)."), QStringLiteral("client_id"));
    QCommandLineOption durOpt(QStringLiteral("duration"), QStringLiteral("Run time in seconds (default 10)."), QStringLiteral("seconds"), QStringLiteral("10"));
    QCommandLineOption discOpt(QStringLiteral("discover-only"), QStringLiteral("Only list discovered radios."));
    parser.addOptions({ipOpt, portOpt, bindOpt, durOpt, discOpt});
    parser.process(app);

    FlexDiscovery discovery;
    FlexRadio radio;
    bool connecting = false;

    auto connectTo = [&](const QHostAddress &addr, quint16 port) {
        if (connecting)
            return;
        connecting = true;
        out() << "Verbinde mit " << addr.toString() << ":" << port << " …\n";
        out().flush();
        radio.connectToRadio(addr, port);
    };

    QObject::connect(&discovery, &FlexDiscovery::radioFound, [&](const RadioInfo &info) {
        out() << "Gefunden: " << info.displayName() << "  SN " << info.serial << "  v" << info.version
              << "  Status " << info.status << "  GUI: " << info.guiClientPrograms.join(QLatin1Char(','))
              << "\n";
        out().flush();
        if (!parser.isSet(discOpt) && !parser.isSet(ipOpt))
            connectTo(info.address, info.port);
    });

    QObject::connect(&radio, &FlexRadio::logMessage, [](const QString &m) { out() << m << "\n"; out().flush(); });
    QObject::connect(&radio, &FlexRadio::errorOccurred, [](const QString &m) { out() << "FEHLER: " << m << "\n"; out().flush(); });
    QObject::connect(&radio, &FlexRadio::boundClientChanged, [&](const QString &id) {
        if (!id.isEmpty())
            printState(radio);
    });

    if (!discovery.start())
        out() << "Hinweis: Discovery-Port 4992/UDP nicht verfügbar.\n";

    if (parser.isSet(bindOpt))
        radio.bindToClient(parser.value(bindOpt));
    if (parser.isSet(ipOpt))
        connectTo(QHostAddress(parser.value(ipOpt)), parser.value(portOpt).toUShort());

    QTimer::singleShot(parser.value(durOpt).toInt() * 1000, &app, [&] {
        if (radio.state() == FlexRadio::State::Connected)
            printState(radio);
        else if (!parser.isSet(discOpt))
            out() << "Keine Verbindung zustande gekommen.\n";
        out().flush();
        radio.disconnectFromRadio();
        app.quit();
    });

    return app.exec();
}
