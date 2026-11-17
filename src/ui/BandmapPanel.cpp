// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/BandmapPanel.h"

#include "contest/Bands.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QSettings>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace qrl {

namespace {
constexpr int ColFreq = 0, ColCall = 1, ColStatus = 2, ColWpm = 3, ColSnr = 4, ColAge = 5, ColSpotter = 6;
}

BandmapPanel::BandmapPanel(std::function<Status(const QString &, const QString &)> statusFor,
                           std::function<double()> currentKhz, QWidget *parent)
    : QWidget(parent)
    , m_statusFor(std::move(statusFor))
    , m_currentKhz(std::move(currentKhz))
    , m_bandFilter(new QComboBox(this))
    , m_onlyNew(new QCheckBox(tr("nur ungearbeitete"), this))
    , m_table(new QTableWidget(0, 7, this))
    , m_status(new QLabel(this))
{
    m_bandFilter->addItem(tr("Aktuelles Band"), QString());
    m_bandFilter->addItem(tr("Alle Bänder"), QStringLiteral("*"));
    for (const char *b : {"160M", "80M", "40M", "20M", "15M", "10M", "30M", "17M", "12M"})
        m_bandFilter->addItem(QString::fromLatin1(b), QString::fromLatin1(b));
    QSettings settings;
    m_bandFilter->setCurrentIndex(qMax(0, m_bandFilter->findData(settings.value(QStringLiteral("bandmap/band")).toString())));
    m_onlyNew->setChecked(settings.value(QStringLiteral("bandmap/onlyNew"), false).toBool());

    m_table->setHorizontalHeaderLabels({tr("kHz"), tr("Call"), tr("Status"), tr("WpM"), tr("dB"), tr("Min"), tr("Spotter")});
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(m_table->fontMetrics().height() + 4);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->installEventFilter(this);
    m_status->setWordWrap(true);

    auto *top = new QHBoxLayout;
    top->addWidget(m_bandFilter);
    top->addWidget(m_onlyNew);
    top->addStretch();
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->addLayout(top);
    layout->addWidget(m_table, 1);
    layout->addWidget(m_status);

    auto emitRow = [this](int row) {
        if (row < 0)
            return;
        const QString key = m_table->item(row, ColCall)->data(Qt::UserRole).toString();
        if (m_spots.contains(key))
            select(m_spots.value(key));
    };
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [emitRow](int row, int) { emitRow(row); });
    connect(m_bandFilter, &QComboBox::currentIndexChanged, this, [this] {
        QSettings().setValue(QStringLiteral("bandmap/band"), m_bandFilter->currentData().toString());
        refresh();
    });
    connect(m_onlyNew, &QCheckBox::toggled, this, [this](bool on) {
        QSettings().setValue(QStringLiteral("bandmap/onlyNew"), on);
        refresh();
    });

    auto *timer = new QTimer(this);
    timer->setInterval(2000);
    connect(timer, &QTimer::timeout, this, &BandmapPanel::refresh);
    timer->start();
}

void BandmapPanel::addSpot(const Spot &spot)
{
    const QString band = bandForKhz(spot.freqKhz);
    if (band.isEmpty())
        return;
    const QString key = spot.call + QLatin1Char('|') + band;
    Spot merged = spot;
    if (m_spots.contains(key)) {
        const Spot &old = m_spots.value(key);
        // keep the last known skimmer speed/SNR if the new spot has none
        if (merged.wpm == 0)
            merged.wpm = old.wpm;
        if (merged.snr == 0 && !merged.skimmer)
            merged.snr = old.snr;
        if (merged.mode.isEmpty())
            merged.mode = old.mode;
    }
    m_spots.insert(key, merged);
}

void BandmapPanel::setConnectionStatus(const QString &text)
{
    m_status->setText(text);
}

QList<Spot> BandmapPanel::visibleSpots() const
{
    QString band = m_bandFilter->currentData().toString();
    if (band.isEmpty())
        band = bandForKhz(m_currentKhz());
    QList<Spot> list;
    for (const Spot &s : m_spots) {
        const QString spotBand = bandForKhz(s.freqKhz);
        if (band != QLatin1String("*") && spotBand != band)
            continue;
        if (!m_modeFilter.isEmpty() && !s.mode.isEmpty() && s.mode != m_modeFilter)
            continue;
        if (m_onlyNew->isChecked() && m_statusFor(s.call, spotBand) == Status::Dupe)
            continue;
        list.append(s);
    }
    std::sort(list.begin(), list.end(), [](const Spot &a, const Spot &b) { return a.freqKhz < b.freqKhz; });
    return list;
}

void BandmapPanel::refresh()
{
    // Expire old spots
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (auto it = m_spots.begin(); it != m_spots.end();) {
        if (it->time.secsTo(now) > m_lifetimeMin * 60)
            it = m_spots.erase(it);
        else
            ++it;
    }

    const QString selectedKey = m_table->currentRow() >= 0 && m_table->item(m_table->currentRow(), ColCall)
        ? m_table->item(m_table->currentRow(), ColCall)->data(Qt::UserRole).toString()
        : QString();
    const QList<Spot> spots = visibleSpots();
    const double current = m_currentKhz();

    m_table->setUpdatesEnabled(false);
    m_table->setRowCount(int(spots.size()));
    int row = 0;
    int nearestRow = -1;
    double nearestDist = 1e9;
    for (const Spot &s : spots) {
        const QString band = bandForKhz(s.freqKhz);
        const Status status = m_statusFor(s.call, band);
        const QString key = s.call + QLatin1Char('|') + band;
        const QString statusText = status == Status::Dupe ? tr("Dupe") : status == Status::NewBand ? tr("neu Band") : tr("NEU");
        const QStringList cells{
            QString::number(s.freqKhz, 'f', 1), s.call, statusText,
            s.wpm > 0 ? QString::number(s.wpm) : QString(),
            s.snr != 0 ? QString::number(s.snr) : QString(),
            QString::number(s.time.secsTo(now) / 60), s.spotter,
        };
        for (int col = 0; col < cells.size(); ++col) {
            auto *item = m_table->item(row, col);
            if (!item) {
                item = new QTableWidgetItem;
                m_table->setItem(row, col, item);
            }
            item->setText(cells.at(col));
            QFont f = item->font();
            f.setBold(status == Status::New && (col == ColCall || col == ColStatus));
            f.setStrikeOut(status == Status::Dupe && col == ColCall);
            item->setFont(f);
            item->setForeground(status == Status::Dupe ? palette().color(QPalette::Disabled, QPalette::Text)
                                : status == Status::New ? QColor(0, 128, 0)
                                                        : palette().color(QPalette::Text));
        }
        m_table->item(row, ColCall)->setData(Qt::UserRole, key);
        if (key == selectedKey)
            m_table->selectRow(row);
        if (current > 0 && qAbs(s.freqKhz - current) < nearestDist) {
            nearestDist = qAbs(s.freqKhz - current);
            nearestRow = row;
        }
        ++row;
    }
    // Mark the spot nearest to the TX frequency
    for (int r = 0; r < m_table->rowCount(); ++r)
        m_table->item(r, ColFreq)->setBackground(r == nearestRow && nearestDist < 0.3 ? QColor(255, 230, 150) : QBrush());
    m_table->resizeColumnsToContents();
    m_table->setUpdatesEnabled(true);
}

void BandmapPanel::select(const Spot &spot)
{
    m_lastJumpKhz = spot.freqKhz;
    m_radioKhzAtJump = m_currentKhz();
    m_lastJumpKey = spot.call + QLatin1Char('|') + bandForKhz(spot.freqKhz);
    Q_EMIT spotSelected(spot);
}

void BandmapPanel::tuneNext(bool up)
{
    // Reference: the spot we jumped to last, as long as the radio is still near it.
    // The slice frequency reported back may lag or differ slightly (tuning step, CW offset),
    // which otherwise makes the jump bounce between two neighbouring spots.
    const double radioKhz = m_currentKhz();
    const bool onLastSpot = m_lastJumpKhz > 0
        && (qAbs(radioKhz - m_lastJumpKhz) < 1.5 || qAbs(radioKhz - m_radioKhzAtJump) < 0.001);
    const double current = onLastSpot ? m_lastJumpKhz : radioKhz;
    const QList<Spot> spots = visibleSpots();
    const Spot *best = nullptr;
    for (const Spot &s : spots) {
        if (m_statusFor(s.call, bandForKhz(s.freqKhz)) == Status::Dupe)
            continue;
        if (onLastSpot && s.call + QLatin1Char('|') + bandForKhz(s.freqKhz) == m_lastJumpKey)
            continue;
        const double d = s.freqKhz - current;
        if (up ? d >= 0.05 : d <= -0.05) {
            if (!best || (up ? s.freqKhz < best->freqKhz : s.freqKhz > best->freqKhz))
                best = &s;
        }
    }
    if (best)
        select(*best);
}

bool BandmapPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_table && event->type() == QEvent::KeyPress) {
        const auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            const int row = m_table->currentRow();
            if (row >= 0) {
                const QString key = m_table->item(row, ColCall)->data(Qt::UserRole).toString();
                if (m_spots.contains(key))
                    select(m_spots.value(key));
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace qrl
