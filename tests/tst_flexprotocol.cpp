// SPDX-License-Identifier: GPL-3.0-or-later
#include "radio/flex/FlexDiscovery.h"
#include "radio/flex/FlexProtocol.h"

#include <QtEndian>
#include <QtTest>

using namespace qrl::flex;

class TestFlexProtocol : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void versionAndHandle()
    {
        Message v = parseLine(QStringLiteral("V1.4.0.0"));
        QCOMPARE(v.type, Message::Type::Version);
        QCOMPARE(v.text, QStringLiteral("1.4.0.0"));

        Message h = parseLine(QStringLiteral("H6F4EC23D"));
        QCOMPARE(h.type, Message::Type::Handle);
        QCOMPARE(h.handle, 0x6F4EC23Du);
    }

    void reply()
    {
        Message r = parseLine(QStringLiteral("R12|0|model=\"FLEX-6700\" callsign=DF1QR"));
        QCOMPARE(r.type, Message::Type::Reply);
        QCOMPARE(r.sequence, 12u);
        QCOMPARE(r.code, 0u);
        QCOMPARE(r.values.value(QStringLiteral("model")), QStringLiteral("FLEX-6700"));
        QCOMPARE(r.values.value(QStringLiteral("callsign")), QStringLiteral("DF1QR"));

        Message err = parseLine(QStringLiteral("R13|50000016|Unknown command"));
        QCOMPARE(err.type, Message::Type::Reply);
        QCOMPARE(err.code, 0x50000016u);
        QCOMPARE(err.text, QStringLiteral("Unknown command"));
    }

    void infoReply()
    {
        Message r = parseLine(QStringLiteral(
            "R3|0|model=\"FLEX-6700\",chassis_serial=\"0520-0844-6700-4789\",name=\"DF1QR\","
            "callsign=\"DF1QR\",gps=\"Locked\",num_scu=2,num_slice=8,software_ver=4.2.20.41343,"
            "options=\"a,b\""));
        const KeyValues kv = parseCommaKeyValues(r.text);
        QCOMPARE(kv.value(QStringLiteral("model")), QStringLiteral("FLEX-6700"));
        QCOMPARE(kv.value(QStringLiteral("name")), QStringLiteral("DF1QR"));
        QCOMPARE(kv.value(QStringLiteral("num_scu")), QStringLiteral("2"));
        QCOMPARE(kv.value(QStringLiteral("software_ver")), QStringLiteral("4.2.20.41343"));
        QCOMPARE(kv.value(QStringLiteral("options")), QStringLiteral("a,b"));
    }

    void statusSlice()
    {
        Message s = parseLine(QStringLiteral("S12345678|slice 0 RF_frequency=14.025000 mode=CW tx=1 index_letter=A"));
        QCOMPARE(s.type, Message::Type::Status);
        QCOMPARE(s.handle, 0x12345678u);
        QCOMPARE(s.object, QStringLiteral("slice 0"));
        QCOMPARE(s.values.value(QStringLiteral("RF_frequency")), QStringLiteral("14.025000"));
        QCOMPARE(s.values.value(QStringLiteral("mode")), QStringLiteral("CW"));
    }

    void statusClient()
    {
        Message s = parseLine(QStringLiteral(
            "S0|client 0x1A2B3C4D connected local_ptt=1 client_id=72E8C7F3-5766-4ADF-9286-EBF2F525C77 program=AetherSDR station=DA2E\x7fShack"));
        QCOMPARE(s.object, QStringLiteral("client 0x1A2B3C4D connected"));
        QCOMPARE(s.values.value(QStringLiteral("program")), QStringLiteral("AetherSDR"));
        QCOMPARE(decodeSpaces(s.values.value(QStringLiteral("station"))), QStringLiteral("DA2E Shack"));
    }

    void statusWithoutValues()
    {
        Message s = parseLine(QStringLiteral("S0|client 0x1A2B3C4D disconnected"));
        QCOMPARE(s.object, QStringLiteral("client 0x1A2B3C4D disconnected"));
        QVERIFY(s.values.isEmpty());
    }

    void infoMessage()
    {
        Message m = parseLine(QStringLiteral("M01000001|Some warning"));
        QCOMPARE(m.type, Message::Type::Info);
        QCOMPARE(m.severity(), 1);
        QCOMPARE(m.text, QStringLiteral("Some warning"));
    }

    void buildsCommand()
    {
        QCOMPARE(buildCommand(7, QStringLiteral("sub slice all")), QByteArray("C7|sub slice all\n"));
    }

    void discoveryPacket()
    {
        const QByteArray text("model=FLEX-6700 serial=1234-5678-6700-0001 version=3.10.15.0 "
                              "nickname=DA2E\x7fRemote callsign=DA2E ip=192.168.1.50 port=4992 "
                              "status=Available gui_client_programs=AetherSDR,Maestro "
                              "gui_client_stations=Shack,Maestro\x7f""1");
        QByteArray payload = text;
        while (payload.size() % 4)
            payload.append('\0');

        const int words = 7 + int(payload.size()) / 4;
        QByteArray packet(28, '\0');
        auto put = [&](int offset, quint32 value) {
            qToBigEndian(value, reinterpret_cast<uchar *>(packet.data()) + offset);
        };
        // type 3 (ext data w/ stream id), class id present, TSI=3, TSF=1
        put(0, (0x3u << 28) | (1u << 27) | (0x3u << 22) | (0x1u << 20) | quint32(words));
        put(4, FlexDiscovery::StreamId);
        put(8, 0x00001C2D);
        put(12, FlexDiscovery::ClassIdLow);
        packet.append(payload);

        const auto extracted = FlexDiscovery::extractPayload(packet);
        QVERIFY(extracted.has_value());
        const RadioInfo info = FlexDiscovery::parsePayload(*extracted);
        QCOMPARE(info.model, QStringLiteral("FLEX-6700"));
        QCOMPARE(info.serial, QStringLiteral("1234-5678-6700-0001"));
        QCOMPARE(info.nickname, QStringLiteral("DA2E Remote"));
        QCOMPARE(info.address.toString(), QStringLiteral("192.168.1.50"));
        QCOMPARE(info.guiClientPrograms, QStringList({QStringLiteral("AetherSDR"), QStringLiteral("Maestro")}));
        QCOMPARE(info.guiClientStations, QStringList({QStringLiteral("Shack"), QStringLiteral("Maestro 1")}));
    }

    void rejectsForeignPacket()
    {
        QByteArray junk(64, '\x11');
        QVERIFY(!FlexDiscovery::extractPayload(junk).has_value());
    }
};

QTEST_GUILESS_MAIN(TestFlexProtocol)
#include "tst_flexprotocol.moc"
