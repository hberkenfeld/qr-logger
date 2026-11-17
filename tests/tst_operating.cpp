// SPDX-License-Identifier: GPL-3.0-or-later
//
// End-to-end test of the operating flow (ESM, CWX, logging) against the
// radio emulator tools/fake_flex.py. Skipped if python3 is not available.
#include "contest/TtcContest.h"
#include "log/LogDatabase.h"
#include "radio/flex/FlexRadio.h"
#include "tx/CwKeyer.h"
#include "ui/ContestPanel.h"
#include "ui/FreeTextDialog.h"
#include "net/ClusterClient.h"
#include "ui/BandmapPanel.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include <QApplication>
#include <QLineEdit>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

using namespace qrl;

Q_DECLARE_METATYPE(qrl::Spot)

class TestOperating : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        const QString python = QStandardPaths::findExecutable(QStringLiteral("python3"));
        if (python.isEmpty())
            QSKIP("python3 not available");
        m_fake.setProcessChannelMode(QProcess::MergedChannels);
        m_fake.start(python, {QStringLiteral(QRL_SOURCE_DIR "/tools/fake_flex.py"), QStringLiteral("--port"),
                              QStringLiteral("0"), QStringLiteral("--no-discovery"), QStringLiteral("--exit-on-stdin-eof")});
        QVERIFY(m_fake.waitForStarted());
        QByteArray banner;
        QTRY_VERIFY_WITH_TIMEOUT((banner += m_fake.readAll(), banner.contains("Fake FLEX-6700 on TCP ")), 5000);
        const QByteArray line = banner.mid(banner.indexOf("on TCP ") + 7);
        m_port = line.left(line.indexOf('\n')).trimmed().toInt();
        QVERIFY(m_port > 0);
    }

    void cleanupTestCase()
    {
        m_fake.kill();
        m_fake.waitForFinished();
    }

    void esmRunFlow()
    {
        QTemporaryDir dir;
        LogDatabase db;
        QVERIFY(db.open(dir.filePath(QStringLiteral("ttc.qrl"))));
        StationSettings st;
        st.contestId = QString::fromLatin1(TtcContest::Id);
        st.myCall = QStringLiteral("DF1QR");
        db.setStation(st);

        flex::FlexRadio radio;
        CwKeyer keyer(&radio);
        radio.connectToRadio(QHostAddress::LocalHost, quint16(m_port));
        QTRY_VERIFY_WITH_TIMEOUT(!radio.boundClientId().isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(radio.txSlice().has_value(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(keyer.speed(), 28, 5000);

        ContestPanel panel(&db, &radio, &keyer);
        panel.show();
        auto *call = panel.findChildren<QLineEdit *>().at(0);
        auto *exch = panel.findChildren<QLineEdit *>().at(2);
        call->setFocus();

        QString output;
        auto drain = [&] { output += QString::fromUtf8(m_fake.readAll()).replace(QChar(0x7f), QLatin1Char(' ')); };

        // Enter on empty call: CQ
        QTest::keyClick(call, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT((drain(), output.contains(QStringLiteral("cwx send \"TEST TTC DF1QR\""))), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!keyer.isSending(), 5000);

        // Call entered: Insert message (call + exchange), focus moves to exchange
        QTest::keyClicks(call, QStringLiteral("sp1abc"));
        QCOMPARE(call->text(), QStringLiteral("SP1ABC"));
        QTest::keyClick(call, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT((drain(), output.contains(QStringLiteral("cwx send \"SP1ABC 5NN 001\""))), 3000);
        QVERIFY(exch->hasFocus());

        // Exchange entered: TU message and log
        QTest::keyClicks(exch, QStringLiteral("12"));
        QTest::keyClick(exch, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT((drain(), output.contains(QStringLiteral("cwx send \"TU DF1QR\""))), 3000);

        const auto qsos = db.qsos();
        QCOMPARE(qsos.size(), 1);
        QCOMPARE(qsos.first().call, QStringLiteral("SP1ABC"));
        QCOMPARE(qsos.first().exchSent, QStringLiteral("001"));
        QCOMPARE(qsos.first().exchRcvd, QStringLiteral("12"));
        QCOMPARE(qsos.first().band, QStringLiteral("20M"));
        QVERIFY(call->text().isEmpty());

        // Frequency in the call field: QSY of the TX slice
        QTest::keyClicks(call, QStringLiteral("7025.5"));
        QTest::keyClick(call, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT((drain(), output.contains(QStringLiteral("slice tune 0 7.025500"))), 3000);
        QVERIFY(call->text().isEmpty());
        QTest::keyClicks(call, QStringLiteral("14.03"));
        QTest::keyClick(call, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT((drain(), output.contains(QStringLiteral("slice tune 0 14.030000"))), 3000);

        // Next serial is 002, speed change via PgUp, Esc stops
        QTest::keyClick(call, Qt::Key_PageUp);
        QTRY_VERIFY_WITH_TIMEOUT((drain(), output.contains(QStringLiteral("cwx wpm 30"))), 3000);
        QTest::keyClick(call, Qt::Key_F2);
        QTRY_VERIFY_WITH_TIMEOUT((drain(), output.contains(QStringLiteral("cwx send \"5NN 002\""))), 3000);
        QTest::keyClick(call, Qt::Key_Escape);
        QTRY_VERIFY_WITH_TIMEOUT((drain(), output.contains(QStringLiteral("cwx clear"))), 3000);

        // PTT hold mode: xmit 1 before, xmit 0 after the message
        QTRY_VERIFY_WITH_TIMEOUT(!keyer.isSending(), 5000);
        keyer.setHoldPtt(true);
        output.clear();
        QTest::keyClick(call, Qt::Key_F3);
        QTRY_VERIFY_WITH_TIMEOUT((drain(), output.contains(QStringLiteral("xmit 0"))), 5000);
        QVERIFY(output.indexOf(QStringLiteral("xmit 1")) < output.indexOf(QStringLiteral("cwx send \"TU\"")));
        QVERIFY(output.indexOf(QStringLiteral("cwx send \"TU\"")) < output.indexOf(QStringLiteral("xmit 0")));

        // Free text: nothing before the first character, then character by character
        QTRY_VERIFY_WITH_TIMEOUT(!keyer.isSending(), 5000);
        keyer.setHoldPtt(false);
        output.clear();
        {
            FreeTextDialog dialog(&keyer, [] { return true; });
            dialog.show();
            auto *edit = dialog.findChild<QLineEdit *>();
            QTest::keyClick(edit, Qt::Key_Space); // leading space is ignored
            QTest::qWait(200);
            drain();
            QVERIFY(!output.contains(QStringLiteral("cwx send")));
            QTest::keyClicks(edit, QStringLiteral("t u"));
            QCOMPARE(dialog.sentText(), QStringLiteral("T U"));
            QTRY_VERIFY_WITH_TIMEOUT((drain(), output.contains(QStringLiteral("cwx send \"U\""))), 3000);
            QVERIFY(output.contains(QStringLiteral("cwx send \"T\"")));
            QTest::keyClick(edit, Qt::Key_Escape);
            QTRY_VERIFY_WITH_TIMEOUT((drain(), output.contains(QStringLiteral("cwx clear"))), 3000);
        }

        radio.disconnectFromRadio();
        db.close();
    }

    void clusterLoginAndSpots()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        ClusterClient client(QStringLiteral("Skimmer"));
        QSignalSpy spots(&client, &ClusterClient::spotReceived);
        client.connectTo(QStringLiteral("127.0.0.1"), server.serverPort(), QStringLiteral("df1qr"));
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 3000);
        QTcpSocket *peer = server.nextPendingConnection();
        peer->write("\xff\xfb\x01Welcome to RBN\r\nPlease enter your call: ");
        QByteArray login;
        QTRY_VERIFY_WITH_TIMEOUT((login += peer->readAll(), login.contains("DF1QR\r\n")), 3000);
        peer->write("DX de DF1QR-#:    7025.1  SP1ABC       CW    22 dB  28 WPM  CQ      1934Z\r\n");
        QTRY_COMPARE_WITH_TIMEOUT(spots.count(), 1, 3000);
        const Spot spot = spots.at(0).at(0).value<Spot>();
        QCOMPARE(spot.call, QStringLiteral("SP1ABC"));
        QCOMPARE(spot.source, QStringLiteral("Skimmer"));
        client.disconnectFrom();
    }

    void bandmapTune()
    {
        flex::FlexRadio radio;
        radio.connectToRadio(QHostAddress::LocalHost, quint16(m_port));
        QTRY_VERIFY_WITH_TIMEOUT(radio.txSlice().has_value(), 5000);

        BandmapPanel bandmap([](const QString &call, const QString &) {
                                 return call == QLatin1String("DL1DUP") ? BandmapPanel::Status::Dupe : BandmapPanel::Status::New;
                             },
                             [&radio] { return radio.txSlice()->frequencyMHz * 1000.0; });
        // radio reports 14025.0 throughout
        Spot a; a.call = QStringLiteral("SP1ABC"); a.freqKhz = 14025.8; a.time = QDateTime::currentDateTimeUtc();
        Spot b = a; b.call = QStringLiteral("DL1DUP"); b.freqKhz = 14026.5;
        Spot c = a; c.call = QStringLiteral("OK1XYZ"); c.freqKhz = 14024.0;
        Spot d = a; d.call = QStringLiteral("K1AR"); d.freqKhz = 7020.0; // other band
        Spot e = a; e.call = QStringLiteral("G4ABC"); e.freqKhz = 14027.5;
        for (const Spot &s : {a, b, c, d, e})
            bandmap.addSpot(s);
        bandmap.refresh();

        QSignalSpy selected(&bandmap, &BandmapPanel::spotSelected);
        // The radio keeps reporting 14025.0 (lag): repeated jumps must walk on, not bounce.
        bandmap.tuneNext(true);   // 14025.0 → SP1ABC 14025.8
        bandmap.tuneNext(true);   // reference SP1ABC → G4ABC 14027.5 (DL1DUP 14026.5 is a dupe)
        bandmap.tuneNext(false);  // reference G4ABC → SP1ABC
        bandmap.tuneNext(false);  // → OK1XYZ 14024.0
        QCOMPARE(selected.count(), 4);
        QCOMPARE(selected.at(0).at(0).value<Spot>().call, QStringLiteral("SP1ABC"));
        QCOMPARE(selected.at(1).at(0).value<Spot>().call, QStringLiteral("G4ABC"));
        QCOMPARE(selected.at(2).at(0).value<Spot>().call, QStringLiteral("SP1ABC"));
        QCOMPARE(selected.at(3).at(0).value<Spot>().call, QStringLiteral("OK1XYZ"));

        QByteArray out;
        radio.tuneSlice(radio.txSlice()->index, 14030.0);
        QTRY_VERIFY_WITH_TIMEOUT((out += m_fake.readAll(), out.contains("slice tune 0 14.030000")), 3000);
        radio.disconnectFromRadio();
    }

private:
    QProcess m_fake;
    int m_port = 0;
};

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestOperating test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_operating.moc"
