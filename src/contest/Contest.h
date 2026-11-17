// SPDX-License-Identifier: GPL-3.0-or-later
//
// Contest rules: dupe check, exchange validation, scoring and default messages.
#pragma once

#include "contest/Qso.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include <memory>

namespace qrl {

struct Score
{
    int qsos = 0;        // valid, non-dupe QSOs
    int dupes = 0;
    int points = 0;
    int multipliers = 0;
    qint64 total = 0;
    QString details;     // contest specific breakdown for display
};

// Message keys as in DXLog: F1..F8, INS (Insert), PLUS (+).
using MessageSet = QMap<QString, QString>;

class Contest
{
public:
    virtual ~Contest() = default;

    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString cabrilloName() const = 0;
    virtual QStringList categoryBands() const = 0;
    virtual QStringList categoryPowers() const { return {QStringLiteral("HIGH"), QStringLiteral("LOW"), QStringLiteral("QRP")}; }
    virtual QStringList cabrilloHeaderKeys() const = 0;

    // Exchange we send, without RST (e.g. serial "001" or CQ zone "14").
    virtual QString exchangeSent(const StationSettings &station, int serial) const = 0;
    virtual QString exchangeLabel() const = 0;
    virtual bool validateExchange(const QString &exchange, QString *error) const = 0;
    virtual bool usesSerial() const = 0;

    // Returns a description if the candidate is a dupe ("Dupe – Stage II"), else empty.
    virtual QString dupeReason(const Qso &candidate, const QList<Qso> &log) const = 0;
    virtual Score score(const QList<Qso> &log, const StationSettings &station) const = 0;

    // Status line text for the current time (e.g. TTC stage), may be empty.
    virtual QString timeStatus(const QDateTime &utc) const { Q_UNUSED(utc); return {}; }

    virtual MessageSet defaultMessages(bool run) const = 0;
};

QStringList availableContests();                    // ids
std::unique_ptr<Contest> createContest(const QString &id);

} // namespace qrl
