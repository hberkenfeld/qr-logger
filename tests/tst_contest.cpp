// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio/Sidetone.h"
#include "contest/Bands.h"
#include "contest/TtcContest.h"
#include "log/Cabrillo.h"
#include "log/LogDatabase.h"
#include "tx/CwKeyer.h"
#include "tx/Macros.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace qrl;

namespace {
// Tuesday 2026-11-17
QDateTime utc(int h, int m, int s = 0)
{
    return QDateTime(QDate(2026, 11, 17), QTime(h, m, s), Qt::UTC);
}

Qso qso(const QString &call, const QString &band, const QDateTime &t, int serial, qint64 id)
{
    Qso q;
    q.id = id;
    q.call = call;
    q.band = band;
    q.time = t;
    q.freqKhz = band == QLatin1String("40M") ? 7025 : 3530;
    q.mode = QStringLiteral("CW");
    q.rstSent = q.rstRcvd = QStringLiteral("599");
    q.exchSent = QStringLiteral("%1").arg(serial, 3, 10, QLatin1Char('0'));
    q.exchRcvd = QStringLiteral("5");
    return q;
}
} // namespace

class TestContest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void bands()
    {
        QCOMPARE(bandForKhz(7025.0), QStringLiteral("40M"));
        QCOMPARE(bandForKhz(3530.0), QStringLiteral("80M"));
        QVERIFY(bandForKhz(5000.0).isEmpty());
    }

    void ttcStages()
    {
        QCOMPARE(TtcContest::stage(utc(19, 29, 59)), 0);
        QCOMPARE(TtcContest::stage(utc(19, 30, 0)), 1);
        QCOMPARE(TtcContest::stage(utc(19, 44, 59)), 1);
        QCOMPARE(TtcContest::stage(utc(19, 45, 0)), 2);
        QCOMPARE(TtcContest::stage(utc(20, 15, 0)), 4);
        QCOMPARE(TtcContest::stage(utc(20, 29, 59)), 4);
        QCOMPARE(TtcContest::stage(utc(20, 30, 0)), 0);
        // Wednesday
        QCOMPARE(TtcContest::stage(QDateTime(QDate(2026, 11, 18), QTime(19, 35), Qt::UTC)), 0);
    }

    void ttcDupesAndScore()
    {
        TtcContest c;
        QList<Qso> log{
            qso(QStringLiteral("SP1ABC"), QStringLiteral("40M"), utc(19, 31), 1, 1),
            qso(QStringLiteral("SP1ABC"), QStringLiteral("40M"), utc(19, 40), 2, 2), // dupe, same stage
            qso(QStringLiteral("SP1ABC"), QStringLiteral("40M"), utc(19, 46), 3, 3), // stage II ok
            qso(QStringLiteral("SP1ABC"), QStringLiteral("80M"), utc(19, 47), 4, 4), // other band ok
            qso(QStringLiteral("DL1XYZ"), QStringLiteral("80M"), utc(19, 48), 5, 5),
        };
        QVERIFY(!c.dupeReason(log.at(1), log.mid(0, 1)).isEmpty());
        QVERIFY(c.dupeReason(log.at(2), log.mid(0, 2)).isEmpty());

        const Score s = c.score(log, StationSettings{});
        QCOMPARE(s.qsos, 4);
        QCOMPARE(s.dupes, 1);
        QCOMPARE(s.multipliers, 2);
        QCOMPARE(s.total, qint64(8));
    }

    void ttcExchange()
    {
        TtcContest c;
        QCOMPARE(c.exchangeSent(StationSettings{}, 7), QStringLiteral("007"));
        QVERIFY(c.validateExchange(QStringLiteral("12"), nullptr));
        QVERIFY(!c.validateExchange(QStringLiteral(""), nullptr));
        QVERIFY(!c.validateExchange(QStringLiteral("ABC"), nullptr));
    }

    void macros()
    {
        MacroContext ctx;
        ctx.myCall = QStringLiteral("DF1QR");
        ctx.call = QStringLiteral("SP1ABC");
        ctx.serial = QStringLiteral("009");
        QCOMPARE(expandMacro(QStringLiteral("TEST TTC $MYCALL"), ctx), QStringLiteral("TEST TTC DF1QR"));
        QCOMPARE(expandMacro(QStringLiteral("$CALL $RST $SERIAL"), ctx), QStringLiteral("SP1ABC 5NN 009"));
        ctx.cutNumbers = true;
        QCOMPARE(expandMacro(QStringLiteral("$RST $SERIAL"), ctx), QStringLiteral("5NN TTN"));
        QCOMPARE(expandMacro(QStringLiteral("$UNKNOWN tu"), ctx), QStringLiteral("$UNKNOWN TU"));
    }

    void morseTiming()
    {
        QCOMPARE(morseUnits(QStringLiteral("PARIS")), 43);
        QCOMPARE(morseUnits(QStringLiteral("PARIS PARIS")), 93);
        QCOMPARE(morseDurationMs(QStringLiteral("E"), 20), 60);
    }

    void sidetoneRender()
    {
        // "E" at 20 WpM = one dit = 60 ms = 2880 samples
        const QByteArray pcm = Sidetone::render(QStringLiteral("E"), 20, 600, 0.5);
        QCOMPARE(pcm.size(), 2880 * 2);
        const QByteArray paris = Sidetone::render(QStringLiteral("PARIS"), 20, 600, 0.5);
        QCOMPARE(paris.size(), 43 * 2880 * 2);
    }

    void cwxEncoding()
    {
        QCOMPARE(CwKeyer::encodeText(QStringLiteral("tu df1qr")), QStringLiteral("TU\x7f" "DF1QR"));
    }

    void cabrilloLine()
    {
        Qso q = qso(QStringLiteral("SP1ABC"), QStringLiteral("40M"), utc(19, 31), 1, 1);
        q.exchRcvd = QStringLiteral("12");
        QCOMPARE(cabrilloQsoLine(q, QStringLiteral("DF1QR")),
                 QStringLiteral("QSO:  7025 CW 2026-11-17 1931 DF1QR         599 001    SP1ABC        599 12"));
    }

    void database()
    {
        QTemporaryDir dir;
        LogDatabase db;
        QVERIFY(db.open(dir.filePath(QStringLiteral("test.qrl"))));
        StationSettings st;
        st.contestId = QString::fromLatin1(TtcContest::Id);
        st.myCall = QStringLiteral("DF1QR");
        st.categoryBand = QStringLiteral("40M");
        QVERIFY(db.setStation(st));
        Qso q = qso(QStringLiteral("SP1ABC"), QStringLiteral("40M"), utc(19, 31), 1, 0);
        QVERIFY(db.addQso(q) > 0);
        const auto list = db.qsos();
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().call, QStringLiteral("SP1ABC"));
        QCOMPARE(list.first().time, utc(19, 31));
        QCOMPARE(db.station().categoryBand, QStringLiteral("40M"));

        TtcContest c;
        const QString cbr = exportCabrillo(c, db.station(), list);
        QVERIFY(cbr.startsWith(QStringLiteral("START-OF-LOG: 3.0\r\nCONTEST: TTC-SPCWC\r\nCALLSIGN: DF1QR\r\n")));
        QVERIFY(cbr.contains(QStringLiteral("CATEGORY-BAND: 40M")));
        QVERIFY(cbr.endsWith(QStringLiteral("END-OF-LOG:\r\n")));
        db.close();
    }
};

QTEST_GUILESS_MAIN(TestContest)
#include "tst_contest.moc"
