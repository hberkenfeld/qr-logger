// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cabrillo 3.0 export. Spec: https://wwrof.org/cabrillo/
#pragma once

#include "contest/Contest.h"

namespace qrl {

QString cabrilloQsoLine(const Qso &qso, const QString &myCall);
QString exportCabrillo(const Contest &contest, const StationSettings &station, const QList<Qso> &log);

} // namespace qrl
