// SPDX-License-Identifier: GPL-3.0-or-later
//
// Line protocol of the SmartSDR TCP/IP API (port 4992).
// Reference: https://github.com/flexradio/smartsdr-api-docs/wiki/SmartSDR-TCPIP-API
#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

namespace qrl::flex {

using KeyValues = QHash<QString, QString>;

struct Message
{
    enum class Type { Unknown, Version, Handle, Reply, Status, Info };

    Type type = Type::Unknown;
    quint32 sequence = 0;   // Reply: command sequence number
    quint32 handle = 0;     // Handle / Status: client handle; Info: message number
    quint32 code = 0;       // Reply: result code (0 = success)
    QString object;         // Status: object name ("slice 0", "client 0x1234 connected")
    QString text;           // Version string, reply message, info text
    KeyValues values;       // Status / Reply: key=value pairs

    int severity() const { return int((handle >> 24) & 0x3); } // Info messages only
};

// Parses one line received from the radio (without trailing CR/LF).
Message parseLine(const QString &line);

// Splits "key=value key2=value2" into a hash. Tokens without '=' are ignored.
// Surrounding double quotes are removed from values.
KeyValues parseKeyValues(const QString &text);

// Splits a comma separated list as used in the "info" reply:
// model="FLEX-6700",name="DF1QR",num_scu=2,... Commas inside quotes are kept.
KeyValues parseCommaKeyValues(const QString &text);

// Builds "C<seq>|<command>\n".
QByteArray buildCommand(quint32 sequence, const QString &command);

// The radio encodes spaces inside values as 0x7F.
QString decodeSpaces(QString value);

} // namespace qrl::flex
