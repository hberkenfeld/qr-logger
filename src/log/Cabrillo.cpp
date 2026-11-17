// SPDX-License-Identifier: GPL-3.0-or-later
#include "log/Cabrillo.h"

#include <QtMath>

namespace qrl {

QString cabrilloQsoLine(const Qso &q, const QString &myCall)
{
    // QSO: freq  mo date       time call          rst exch   call          rst exch
    return QStringLiteral("QSO: %1 %2 %3 %4 %5 %6 %7 %8 %9 %10")
        .arg(qRound(q.freqKhz), 5)
        .arg(q.mode, -2)
        .arg(q.time.toUTC().toString(QStringLiteral("yyyy-MM-dd")))
        .arg(q.time.toUTC().toString(QStringLiteral("HHmm")))
        .arg(myCall, -13)
        .arg(q.rstSent, -3)
        .arg(q.exchSent, -6)
        .arg(q.call, -13)
        .arg(q.rstRcvd, -3)
        .arg(q.exchRcvd, -6)
        .trimmed();
}

QString exportCabrillo(const Contest &contest, const StationSettings &st, const QList<Qso> &log)
{
    const QMap<QString, QString> values{
        {QStringLiteral("CALLSIGN"), st.myCall},
        {QStringLiteral("CATEGORY-OPERATOR"), st.categoryOperator},
        {QStringLiteral("CATEGORY-POWER"), st.categoryPower},
        {QStringLiteral("CATEGORY-BAND"), st.categoryBand},
        {QStringLiteral("CATEGORY-MODE"), st.categoryMode},
        {QStringLiteral("CLUB"), st.club},
        {QStringLiteral("EMAIL"), st.email},
        {QStringLiteral("NAME"), st.name},
    };

    QStringList lines;
    lines << QStringLiteral("START-OF-LOG: 3.0");
    lines << QStringLiteral("CONTEST: %1").arg(contest.cabrilloName());
    for (const QString &key : contest.cabrilloHeaderKeys()) {
        const QString value = values.value(key);
        if (!value.isEmpty())
            lines << QStringLiteral("%1: %2").arg(key, value);
    }
    lines << QStringLiteral("CLAIMED-SCORE: %1").arg(contest.score(log, st).total);
    lines << QStringLiteral("CREATED-BY: QR-Logger %1").arg(QStringLiteral(QRL_VERSION));
    for (const Qso &q : log)
        lines << cabrilloQsoLine(q, st.myCall);
    lines << QStringLiteral("END-OF-LOG:");
    return lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
}

} // namespace qrl
