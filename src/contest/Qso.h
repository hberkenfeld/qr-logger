// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QString>

namespace qrl {

struct Qso
{
    qint64 id = 0;
    QDateTime time;          // UTC
    double freqKhz = 0.0;
    QString band;            // "40M", "80M", ...
    QString mode;            // "CW", "PH", "RY"
    QString call;
    QString rstSent;
    QString exchSent;
    QString rstRcvd;
    QString exchRcvd;
};

// Station and Cabrillo category data of a log.
struct StationSettings
{
    QString contestId;
    QString myCall;
    QString categoryOperator = QStringLiteral("SINGLE-OP");
    QString categoryBand = QStringLiteral("ALL");
    QString categoryPower = QStringLiteral("HIGH");
    QString categoryMode = QStringLiteral("CW");
    QString club;
    QString email;
    QString name;
    int cqZone = 14;
};

} // namespace qrl
