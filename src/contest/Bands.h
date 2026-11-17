// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace qrl {

// Cabrillo/ADIF style band name ("160M" … "10M") for a frequency in kHz,
// empty if outside the HF contest bands.
QString bandForKhz(double khz);

} // namespace qrl
