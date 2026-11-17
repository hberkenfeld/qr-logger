// SPDX-License-Identifier: GPL-3.0-or-later
#include "contest/Bands.h"

namespace qrl {

QString bandForKhz(double khz)
{
    struct Range { double lo, hi; const char *name; };
    static constexpr Range ranges[] = {
        {1800, 2000, "160M"}, {3500, 4000, "80M"},  {5250, 5450, "60M"},
        {7000, 7300, "40M"},  {10100, 10150, "30M"}, {14000, 14350, "20M"},
        {18068, 18168, "17M"}, {21000, 21450, "15M"}, {24890, 24990, "12M"},
        {28000, 29700, "10M"}, {50000, 54000, "6M"},
    };
    for (const auto &r : ranges) {
        if (khz >= r.lo && khz <= r.hi)
            return QString::fromLatin1(r.name);
    }
    return {};
}

} // namespace qrl
