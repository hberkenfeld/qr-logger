// SPDX-License-Identifier: GPL-3.0-or-later
#include "radio/flex/FlexProtocol.h"

namespace qrl::flex {

namespace {

QString unquote(QString value)
{
    if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
        return value.mid(1, value.size() - 2);
    return value;
}

// Status body: "<object words> key=value ...". Object tokens never contain '='.
void splitObjectAndValues(const QString &body, QString &object, KeyValues &values)
{
    const int firstEq = body.indexOf(QLatin1Char('='));
    if (firstEq < 0) {
        object = body.trimmed();
        return;
    }
    const int kvStart = body.lastIndexOf(QLatin1Char(' '), firstEq);
    if (kvStart < 0) {
        values = parseKeyValues(body);
        return;
    }
    object = body.left(kvStart).trimmed();
    values = parseKeyValues(body.mid(kvStart + 1));
}

} // namespace

KeyValues parseKeyValues(const QString &text)
{
    KeyValues values;
    const auto tokens = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        const int eq = token.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        values.insert(token.left(eq), unquote(token.mid(eq + 1)));
    }
    return values;
}

Message parseLine(const QString &line)
{
    Message msg;
    if (line.isEmpty())
        return msg;

    const QChar kind = line.at(0);
    const QString body = line.mid(1);

    if (kind == QLatin1Char('V')) {
        msg.type = Message::Type::Version;
        msg.text = body.trimmed();
    } else if (kind == QLatin1Char('H')) {
        bool ok = false;
        msg.handle = body.trimmed().toUInt(&ok, 16);
        if (ok)
            msg.type = Message::Type::Handle;
    } else if (kind == QLatin1Char('R')) {
        // R<seq>|<hex code>|<message>[|<debug>]
        const QStringList parts = body.split(QLatin1Char('|'));
        bool okSeq = false;
        bool okCode = false;
        if (parts.size() >= 2) {
            msg.sequence = parts.at(0).toUInt(&okSeq);
            msg.code = parts.at(1).toUInt(&okCode, 16);
        }
        if (okSeq && okCode) {
            msg.type = Message::Type::Reply;
            if (parts.size() >= 3) {
                msg.text = parts.at(2);
                msg.values = parseKeyValues(parts.at(2));
            }
        }
    } else if (kind == QLatin1Char('S')) {
        // S<handle>|<object> key=value ...
        const int pipe = body.indexOf(QLatin1Char('|'));
        if (pipe > 0) {
            bool ok = false;
            msg.handle = body.left(pipe).toUInt(&ok, 16);
            if (ok) {
                msg.type = Message::Type::Status;
                splitObjectAndValues(body.mid(pipe + 1), msg.object, msg.values);
            }
        }
    } else if (kind == QLatin1Char('M')) {
        // M<hex message number>|<text>
        const int pipe = body.indexOf(QLatin1Char('|'));
        if (pipe > 0) {
            bool ok = false;
            msg.handle = body.left(pipe).toUInt(&ok, 16);
            if (ok) {
                msg.type = Message::Type::Info;
                msg.text = body.mid(pipe + 1);
            }
        }
    }
    return msg;
}

QByteArray buildCommand(quint32 sequence, const QString &command)
{
    return QStringLiteral("C%1|%2\n").arg(sequence).arg(command).toUtf8();
}

QString decodeSpaces(QString value)
{
    return value.replace(QChar(0x7f), QLatin1Char(' ')).trimmed();
}

} // namespace qrl::flex
