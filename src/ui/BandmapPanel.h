// SPDX-License-Identifier: GPL-3.0-or-later
//
// Spot list per band (cluster + skimmer) with contest status. Double-click or
// Enter tunes the TX slice to the spot; Ctrl-Up/Down in the entry line jumps
// to the next unworked spot above/below.
#pragma once

#include "net/ClusterClient.h"

#include <QMap>
#include <QWidget>

#include <functional>

class QCheckBox;
class QComboBox;
class QTableWidget;
class QLabel;

namespace qrl {

class BandmapPanel : public QWidget
{
    Q_OBJECT

public:
    enum class Status { New, NewBand, Dupe };

    // statusFor: contest status of a call on a band (dupe check etc.)
    // currentKhz: frequency of the TX slice
    BandmapPanel(std::function<Status(const QString &call, const QString &band)> statusFor,
                 std::function<double()> currentKhz, QWidget *parent = nullptr);

    void addSpot(const Spot &spot);
    void setLifetimeMinutes(int minutes) { m_lifetimeMin = qMax(1, minutes); }
    void setModeFilter(const QString &mode) { m_modeFilter = mode; }
    void setConnectionStatus(const QString &text);
    void refresh();

    // Selects the next visible, unworked spot above (up) or below the current frequency.
    void tuneNext(bool up);

Q_SIGNALS:
    void spotSelected(const qrl::Spot &spot);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QList<Spot> visibleSpots() const;

    std::function<Status(const QString &, const QString &)> m_statusFor;
    std::function<double()> m_currentKhz;
    QMap<QString, Spot> m_spots; // key: call|band
    QComboBox *m_bandFilter;
    QCheckBox *m_onlyNew;
    QTableWidget *m_table;
    QLabel *m_status;
    QString m_modeFilter = QStringLiteral("CW");
    int m_lifetimeMin = 30;
    double m_lastJumpKhz = 0.0;
    double m_radioKhzAtJump = 0.0;
    QString m_lastJumpKey;

    void select(const Spot &spot);
};

} // namespace qrl
