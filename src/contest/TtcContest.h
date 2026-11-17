// SPDX-License-Identifier: GPL-3.0-or-later
//
// TTC-SPCWC (Tuesday Test Contest of the Polish CW Club).
// Rules: https://spcwc.pl/ttc/rules/en (version 20261024_01)
//  - Tuesdays 19:30:00–20:29:59 UTC, four 15-minute stages
//  - 80 m (3520–3555 kHz) and 40 m (7010–7040 kHz), CW only
//  - Exchange: RST + serial from 001, continuous across stages and bands
//  - Same station once per band per stage
//  - 1 point per QSO, multiplier = unique callsigns, score = QSO × U
#pragma once

#include "contest/Contest.h"

namespace qrl {

class TtcContest : public Contest
{
public:
    static constexpr auto Id = "TTC-SPCWC";

    QString id() const override { return QString::fromLatin1(Id); }
    QString name() const override { return QStringLiteral("TTC – SPCWC Tuesday Test Contest"); }
    QString cabrilloName() const override { return QString::fromLatin1(Id); }
    QStringList categoryBands() const override { return {QStringLiteral("ALL"), QStringLiteral("40M"), QStringLiteral("80M")}; }
    QStringList cabrilloHeaderKeys() const override;

    QString exchangeSent(const StationSettings &station, int serial) const override;
    QString exchangeLabel() const override { return QStringLiteral("Nr"); }
    bool validateExchange(const QString &exchange, QString *error) const override;
    bool usesSerial() const override { return true; }

    QString dupeReason(const Qso &candidate, const QList<Qso> &log) const override;
    Score score(const QList<Qso> &log, const StationSettings &station) const override;
    QString timeStatus(const QDateTime &utc) const override;

    MessageSet defaultMessages(bool run) const override;

    // 1..4 inside the round, 0 outside.
    static int stage(const QDateTime &utc);
    static QString stageName(int stage);
};

} // namespace qrl
