// SPDX-License-Identifier: GPL-3.0-or-later
#include "log/LogDatabase.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace qrl {

namespace {
constexpr int SchemaVersion = 1;
const QString TimeFormat = QStringLiteral("yyyy-MM-ddTHH:mm:ssZ");
} // namespace

LogDatabase::LogDatabase(QObject *parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("qrl-log-%1").arg(QUuid::createUuid().toString(QUuid::Id128)))
{
}

LogDatabase::~LogDatabase()
{
    close();
}

bool LogDatabase::open(const QString &path, QString *error)
{
    close();
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    q.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    if (!createSchema()) {
        if (error)
            *error = m_db.lastError().text();
        close();
        return false;
    }
    m_path = path;
    Q_EMIT changed();
    return true;
}

void LogDatabase::close()
{
    if (m_db.isValid()) {
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    m_path.clear();
}

bool LogDatabase::createSchema()
{
    QSqlQuery q(m_db);
    const bool ok =
        q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT)"))
        && q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS qso ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " time TEXT NOT NULL, freq_khz REAL, band TEXT, mode TEXT, call TEXT NOT NULL,"
            " rst_sent TEXT, exch_sent TEXT, rst_rcvd TEXT, exch_rcvd TEXT)"))
        && q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS qso_call ON qso(call)"));
    if (ok && meta(QStringLiteral("schema")).isEmpty())
        setMeta(QStringLiteral("schema"), QString::number(SchemaVersion));
    return ok;
}

QString LogDatabase::meta(const QString &key, const QString &fallback) const
{
    if (!m_db.isOpen())
        return fallback;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT value FROM meta WHERE key = ?"));
    q.addBindValue(key);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return fallback;
}

bool LogDatabase::setMeta(const QString &key, const QString &value)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)"));
    q.addBindValue(key);
    q.addBindValue(value);
    return q.exec();
}

StationSettings LogDatabase::station() const
{
    StationSettings s;
    s.contestId = meta(QStringLiteral("contest"));
    s.myCall = meta(QStringLiteral("mycall"));
    s.categoryOperator = meta(QStringLiteral("cat_operator"), s.categoryOperator);
    s.categoryBand = meta(QStringLiteral("cat_band"), s.categoryBand);
    s.categoryPower = meta(QStringLiteral("cat_power"), s.categoryPower);
    s.categoryMode = meta(QStringLiteral("cat_mode"), s.categoryMode);
    s.club = meta(QStringLiteral("club"));
    s.email = meta(QStringLiteral("email"));
    s.name = meta(QStringLiteral("name"));
    s.cqZone = meta(QStringLiteral("cq_zone"), QString::number(s.cqZone)).toInt();
    return s;
}

bool LogDatabase::setStation(const StationSettings &s)
{
    bool ok = setMeta(QStringLiteral("contest"), s.contestId);
    ok &= setMeta(QStringLiteral("mycall"), s.myCall);
    ok &= setMeta(QStringLiteral("cat_operator"), s.categoryOperator);
    ok &= setMeta(QStringLiteral("cat_band"), s.categoryBand);
    ok &= setMeta(QStringLiteral("cat_power"), s.categoryPower);
    ok &= setMeta(QStringLiteral("cat_mode"), s.categoryMode);
    ok &= setMeta(QStringLiteral("club"), s.club);
    ok &= setMeta(QStringLiteral("email"), s.email);
    ok &= setMeta(QStringLiteral("name"), s.name);
    ok &= setMeta(QStringLiteral("cq_zone"), QString::number(s.cqZone));
    Q_EMIT changed();
    return ok;
}

qint64 LogDatabase::addQso(Qso &qso)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO qso (time, freq_khz, band, mode, call, rst_sent, exch_sent, rst_rcvd, exch_rcvd)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(qso.time.toUTC().toString(TimeFormat));
    q.addBindValue(qso.freqKhz);
    q.addBindValue(qso.band);
    q.addBindValue(qso.mode);
    q.addBindValue(qso.call);
    q.addBindValue(qso.rstSent);
    q.addBindValue(qso.exchSent);
    q.addBindValue(qso.rstRcvd);
    q.addBindValue(qso.exchRcvd);
    if (!q.exec())
        return 0;
    qso.id = q.lastInsertId().toLongLong();
    Q_EMIT changed();
    return qso.id;
}

bool LogDatabase::updateQso(const Qso &qso)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE qso SET time=?, freq_khz=?, band=?, mode=?, call=?, rst_sent=?, exch_sent=?,"
        " rst_rcvd=?, exch_rcvd=? WHERE id=?"));
    q.addBindValue(qso.time.toUTC().toString(TimeFormat));
    q.addBindValue(qso.freqKhz);
    q.addBindValue(qso.band);
    q.addBindValue(qso.mode);
    q.addBindValue(qso.call);
    q.addBindValue(qso.rstSent);
    q.addBindValue(qso.exchSent);
    q.addBindValue(qso.rstRcvd);
    q.addBindValue(qso.exchRcvd);
    q.addBindValue(qso.id);
    const bool ok = q.exec();
    if (ok)
        Q_EMIT changed();
    return ok;
}

bool LogDatabase::deleteQso(qint64 id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM qso WHERE id=?"));
    q.addBindValue(id);
    const bool ok = q.exec();
    if (ok)
        Q_EMIT changed();
    return ok;
}

QList<Qso> LogDatabase::qsos() const
{
    QList<Qso> list;
    if (!m_db.isOpen())
        return list;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT id, time, freq_khz, band, mode, call, rst_sent, exch_sent, rst_rcvd, exch_rcvd"
        " FROM qso ORDER BY time, id"));
    while (q.next()) {
        Qso qso;
        qso.id = q.value(0).toLongLong();
        qso.time = QDateTime::fromString(q.value(1).toString(), Qt::ISODate).toUTC();
        qso.freqKhz = q.value(2).toDouble();
        qso.band = q.value(3).toString();
        qso.mode = q.value(4).toString();
        qso.call = q.value(5).toString();
        qso.rstSent = q.value(6).toString();
        qso.exchSent = q.value(7).toString();
        qso.rstRcvd = q.value(8).toString();
        qso.exchRcvd = q.value(9).toString();
        list.append(qso);
    }
    return list;
}

int LogDatabase::count() const
{
    if (!m_db.isOpen())
        return 0;
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM qso")) && q.next())
        return q.value(0).toInt();
    return 0;
}

} // namespace qrl
