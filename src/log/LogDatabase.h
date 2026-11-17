// SPDX-License-Identifier: GPL-3.0-or-later
//
// One contest log = one SQLite file. Every QSO is written immediately (WAL mode).
#pragma once

#include "contest/Qso.h"

#include <QList>
#include <QObject>
#include <QSqlDatabase>

namespace qrl {

class LogDatabase : public QObject
{
    Q_OBJECT

public:
    explicit LogDatabase(QObject *parent = nullptr);
    ~LogDatabase() override;

    bool open(const QString &path, QString *error = nullptr);
    void close();
    bool isOpen() const { return m_db.isOpen(); }
    QString path() const { return m_path; }
    QSqlDatabase database() const { return m_db; }

    StationSettings station() const;
    bool setStation(const StationSettings &settings);

    qint64 addQso(Qso &qso);
    bool updateQso(const Qso &qso);
    bool deleteQso(qint64 id);
    QList<Qso> qsos() const;       // ordered by time
    int count() const;

Q_SIGNALS:
    void changed();
    // Emitted before the connection is closed: users of database() must release it.
    void aboutToClose();

private:
    bool createSchema();
    QString meta(const QString &key, const QString &fallback = {}) const;
    bool setMeta(const QString &key, const QString &value);

    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_path;
};

} // namespace qrl
