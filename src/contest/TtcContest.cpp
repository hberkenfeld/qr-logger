// SPDX-License-Identifier: GPL-3.0-or-later
#include "contest/TtcContest.h"

#include <QSet>

namespace qrl {

namespace {
constexpr int RoundStartSecs = 19 * 3600 + 30 * 60; // 19:30:00 UTC
constexpr int StageSecs = 15 * 60;

bool inBandSegment(double khz)
{
    return (khz >= 3520 && khz <= 3555) || (khz >= 7010 && khz <= 7040);
}
} // namespace

QStringList TtcContest::cabrilloHeaderKeys() const
{
    return {QStringLiteral("CALLSIGN"), QStringLiteral("CATEGORY-OPERATOR"), QStringLiteral("CATEGORY-POWER"),
            QStringLiteral("CATEGORY-BAND"), QStringLiteral("CATEGORY-MODE"), QStringLiteral("CLUB"),
            QStringLiteral("EMAIL")};
}

QString TtcContest::exchangeSent(const StationSettings &, int serial) const
{
    return QStringLiteral("%1").arg(serial, 3, 10, QLatin1Char('0'));
}

bool TtcContest::validateExchange(const QString &exchange, QString *error) const
{
    bool ok = false;
    const int nr = exchange.trimmed().toInt(&ok);
    if (!ok || nr <= 0) {
        if (error)
            *error = QStringLiteral("Seriennummer fehlt");
        return false;
    }
    return true;
}

int TtcContest::stage(const QDateTime &utc)
{
    if (utc.date().dayOfWeek() != Qt::Tuesday)
        return 0;
    const int secs = utc.time().msecsSinceStartOfDay() / 1000 - RoundStartSecs;
    if (secs < 0 || secs >= 4 * StageSecs)
        return 0;
    return secs / StageSecs + 1;
}

QString TtcContest::stageName(int stage)
{
    static const char *names[] = {"–", "I", "II", "III", "IV"};
    return QString::fromLatin1(names[qBound(0, stage, 4)]);
}

QString TtcContest::dupeReason(const Qso &candidate, const QList<Qso> &log) const
{
    const int st = stage(candidate.time);
    for (const Qso &q : log) {
        if (q.id == candidate.id || q.call != candidate.call || q.band != candidate.band)
            continue;
        if (stage(q.time) == st)
            return QStringLiteral("Dupe – %1 Stage %2").arg(candidate.band, stageName(st));
    }
    return {};
}

Score TtcContest::score(const QList<Qso> &log, const StationSettings &) const
{
    Score s;
    QSet<QString> calls;
    QList<Qso> seen;
    int perStage[5] = {0, 0, 0, 0, 0};
    int outOfSegment = 0;
    for (const Qso &q : log) {
        if (!dupeReason(q, seen).isEmpty()) {
            ++s.dupes;
            continue;
        }
        seen.append(q);
        ++s.qsos;
        calls.insert(q.call);
        ++perStage[stage(q.time)];
        if (!inBandSegment(q.freqKhz))
            ++outOfSegment;
    }
    s.points = s.qsos;
    s.multipliers = int(calls.size());
    s.total = qint64(s.points) * s.multipliers;
    s.details = QStringLiteral("Stage I %1 · II %2 · III %3 · IV %4")
                    .arg(perStage[1]).arg(perStage[2]).arg(perStage[3]).arg(perStage[4]);
    if (perStage[0])
        s.details += QStringLiteral(" · außerhalb der Runde %1").arg(perStage[0]);
    if (outOfSegment)
        s.details += QStringLiteral(" · außerhalb Bandsegment %1").arg(outOfSegment);
    return s;
}

QString TtcContest::timeStatus(const QDateTime &utc) const
{
    const int st = stage(utc);
    if (st == 0)
        return QStringLiteral("TTC: außerhalb der Runde (Di 19:30–20:30 UTC)");
    const int secs = utc.time().msecsSinceStartOfDay() / 1000 - RoundStartSecs;
    const int left = st * StageSecs - secs;
    return QStringLiteral("Stage %1 · noch %2:%3")
        .arg(stageName(st))
        .arg(left / 60)
        .arg(left % 60, 2, 10, QLatin1Char('0'));
}

MessageSet TtcContest::defaultMessages(bool run) const
{
    if (run) {
        return {
            {QStringLiteral("F1"), QStringLiteral("TEST TTC $MYCALL")},
            {QStringLiteral("F2"), QStringLiteral("$RST $SERIAL")},
            {QStringLiteral("F3"), QStringLiteral("TU")},
            {QStringLiteral("F4"), QStringLiteral("$MYCALL")},
            {QStringLiteral("F5"), QStringLiteral("$CALL")},
            {QStringLiteral("F6"), QStringLiteral("AGN")},
            {QStringLiteral("F7"), QStringLiteral("?")},
            {QStringLiteral("F8"), QStringLiteral("NR?")},
            {QStringLiteral("INS"), QStringLiteral("$CALL $RST $SERIAL")},
            {QStringLiteral("PLUS"), QStringLiteral("TU $MYCALL")},
        };
    }
    return {
        {QStringLiteral("F1"), QStringLiteral("$MYCALL")},
        {QStringLiteral("F2"), QStringLiteral("TU $RST $SERIAL")},
        {QStringLiteral("F3"), QStringLiteral("TU")},
        {QStringLiteral("F4"), QStringLiteral("$MYCALL")},
        {QStringLiteral("F5"), QStringLiteral("$CALL")},
        {QStringLiteral("F6"), QStringLiteral("AGN")},
        {QStringLiteral("F7"), QStringLiteral("?")},
        {QStringLiteral("F8"), QStringLiteral("NR?")},
        {QStringLiteral("INS"), QStringLiteral("$MYCALL")},
        {QStringLiteral("PLUS"), QStringLiteral("TU $RST $SERIAL")},
    };
}

// --- Registry ------------------------------------------------------------------

QStringList availableContests()
{
    return {QString::fromLatin1(TtcContest::Id)};
}

std::unique_ptr<Contest> createContest(const QString &id)
{
    if (id == QLatin1String(TtcContest::Id))
        return std::make_unique<TtcContest>();
    return nullptr;
}

} // namespace qrl
