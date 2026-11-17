// SPDX-License-Identifier: GPL-3.0-or-later
//
// Message macro expansion. Variable names follow DXLog where known
// ($MYCALL, $RST, $SERIAL, $CQZONE, $EXCHANGE); $CALL is the call in the
// entry field, $LASTCALL the call of the last logged QSO.
#pragma once

#include <QString>

namespace qrl {

struct MacroContext
{
    QString myCall;
    QString call;
    QString lastCall;
    QString rst = QStringLiteral("599");
    QString serial;
    QString exchange;
    int cqZone = 0;
    bool cutNumbers = false;  // 0 → T, 9 → N in serial/zone/exchange
};

QString expandMacro(const QString &text, const MacroContext &ctx);

// CW: "599" → "5NN"; with cut numbers also 0 → T and 9 → N.
QString cutRst(const QString &rst);
QString cutDigits(const QString &digits);

// Duration of the text in Morse units (dit = 1), PARIS timing.
int morseUnits(const QString &text);
inline int morseDurationMs(const QString &text, int wpm) { return wpm > 0 ? morseUnits(text) * 1200 / wpm : 0; }

} // namespace qrl
