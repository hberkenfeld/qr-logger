// SPDX-License-Identifier: GPL-3.0-or-later
#include "tx/Macros.h"

#include <QHash>
#include <QRegularExpression>

namespace qrl {

QString cutDigits(const QString &digits)
{
    QString out = digits;
    out.replace(QLatin1Char('0'), QLatin1Char('T'));
    out.replace(QLatin1Char('9'), QLatin1Char('N'));
    return out;
}

QString cutRst(const QString &rst)
{
    QString out = rst;
    out.replace(QLatin1Char('9'), QLatin1Char('N'));
    return out;
}

QString expandMacro(const QString &text, const MacroContext &ctx)
{
    const auto num = [&](const QString &v) { return ctx.cutNumbers ? cutDigits(v) : v; };
    const QHash<QString, QString> vars{
        {QStringLiteral("MYCALL"), ctx.myCall},
        {QStringLiteral("CALL"), ctx.call},
        {QStringLiteral("LASTCALL"), ctx.lastCall},
        {QStringLiteral("RST"), cutRst(ctx.rst)},
        {QStringLiteral("SERIAL"), num(ctx.serial)},
        {QStringLiteral("EXCHANGE"), num(ctx.exchange)},
        {QStringLiteral("CQZONE"), num(QStringLiteral("%1").arg(ctx.cqZone, 2, 10, QLatin1Char('0')))},
    };
    static const QRegularExpression varRe(QStringLiteral("\\$([A-Z_]+)"));
    QString out;
    qsizetype last = 0;
    auto it = varRe.globalMatch(text.toUpper());
    while (it.hasNext()) {
        const auto m = it.next();
        out += text.mid(last, m.capturedStart() - last);
        const auto found = vars.constFind(m.captured(1));
        out += found != vars.constEnd() ? found.value() : m.captured(0);
        last = m.capturedEnd();
    }
    out += text.mid(last);
    return out.toUpper().simplified();
}

const char *morsePattern(QChar ch)
{
    static const QHash<QChar, const char *> table{
        {QLatin1Char('A'), ".-"},    {QLatin1Char('B'), "-..."},  {QLatin1Char('C'), "-.-."},
        {QLatin1Char('D'), "-.."},   {QLatin1Char('E'), "."},     {QLatin1Char('F'), "..-."},
        {QLatin1Char('G'), "--."},   {QLatin1Char('H'), "...."},  {QLatin1Char('I'), ".."},
        {QLatin1Char('J'), ".---"},  {QLatin1Char('K'), "-.-"},   {QLatin1Char('L'), ".-.."},
        {QLatin1Char('M'), "--"},    {QLatin1Char('N'), "-."},    {QLatin1Char('O'), "---"},
        {QLatin1Char('P'), ".--."},  {QLatin1Char('Q'), "--.-"},  {QLatin1Char('R'), ".-."},
        {QLatin1Char('S'), "..."},   {QLatin1Char('T'), "-"},     {QLatin1Char('U'), "..-"},
        {QLatin1Char('V'), "...-"},  {QLatin1Char('W'), ".--"},   {QLatin1Char('X'), "-..-"},
        {QLatin1Char('Y'), "-.--"},  {QLatin1Char('Z'), "--.."},  {QLatin1Char('0'), "-----"},
        {QLatin1Char('1'), ".----"}, {QLatin1Char('2'), "..---"}, {QLatin1Char('3'), "...--"},
        {QLatin1Char('4'), "....-"}, {QLatin1Char('5'), "....."}, {QLatin1Char('6'), "-...."},
        {QLatin1Char('7'), "--..."}, {QLatin1Char('8'), "---.."}, {QLatin1Char('9'), "----."},
        {QLatin1Char('/'), "-..-."}, {QLatin1Char('?'), "..--.."}, {QLatin1Char('='), "-...-"},
        {QLatin1Char('.'), ".-.-.-"}, {QLatin1Char(','), "--..--"}, {QLatin1Char('+'), ".-.-."},
    };
    const auto it = table.constFind(ch.toUpper());
    return it == table.constEnd() ? nullptr : it.value();
}

int morseUnits(const QString &text)
{
    int units = 0;
    bool first = true;
    for (const QChar ch : text.toUpper()) {
        if (ch == QLatin1Char(' ')) {
            units += 4; // word gap 7 = 3 (char gap already counted) + 4
            continue;
        }
        const char *code = morsePattern(ch);
        if (!code)
            continue;
        if (!first)
            units += 3;
        first = false;
        for (int i = 0; code[i]; ++i) {
            if (i > 0)
                units += 1;
            units += code[i] == '.' ? 1 : 3;
        }
    }
    return units;
}

} // namespace qrl
