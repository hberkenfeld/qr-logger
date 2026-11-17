// SPDX-License-Identifier: GPL-3.0-or-later
//
// Central operating widget: log view, entry line (call / RST / exchange),
// DXLog-style keyboard handling and ESM (Enter Sends Message).
#pragma once

#include "contest/Contest.h"

#include <QWidget>

#include <memory>

class QLabel;
class QLineEdit;
class QSpinBox;
class QSqlTableModel;
class QTableView;

namespace qrl {

class CwKeyer;
class LogDatabase;

namespace flex {
class FlexRadio;
}

class ContestPanel : public QWidget
{
    Q_OBJECT

public:
    ContestPanel(LogDatabase *log, flex::FlexRadio *radio, CwKeyer *keyer, QWidget *parent = nullptr);
    ~ContestPanel() override;

    // Called after a log was opened or its settings changed.
    void reload();
    const Contest *contest() const { return m_contest.get(); }

    bool isRun() const { return m_run; }
    bool esmEnabled() const { return m_esm; }

    const QList<Qso> &qsos() const { return m_qsos; }
    const StationSettings &station() const { return m_station; }
    // Takes a call from the bandmap: fills the call field and switches to S&P.
    void takeSpot(const QString &call);

    MessageSet messages(bool run) const;
    void setMessages(bool run, const MessageSet &messages);

Q_SIGNALS:
    void statusMessage(const QString &text);
    void scoreChanged(const qrl::Score &score);
    void nextSpotRequested(bool up);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool handleKey(QLineEdit *field, int key, Qt::KeyboardModifiers mods);
    void sendMessage(const QString &key);
    bool checkCwReady();
    void openFreeText();
    void editMessage(const QString &key);
    void onEnter();
    bool tryQsy();
    bool logQso();
    void wipe();
    void deleteSelectedQso();
    void toggleRun();
    void toggleEsm();
    void toggleCqRepeat();
    void updateInfo();
    void updateScore();
    void updateModeLabel();
    void focusField(QLineEdit *field);
    int nextSerial() const;
    QString currentBand() const;
    double currentFreqKhz() const;

    LogDatabase *m_log;
    flex::FlexRadio *m_radio;
    CwKeyer *m_keyer;
    std::unique_ptr<Contest> m_contest;
    StationSettings m_station;
    QList<Qso> m_qsos;

    QSqlTableModel *m_model = nullptr;
    QTableView *m_view;
    QLineEdit *m_call;
    QLineEdit *m_rst;
    QLineEdit *m_exch;
    QLabel *m_sentLabel;
    QLabel *m_infoLabel;
    QLabel *m_modeLabel;
    QLabel *m_txLabel;
    QSpinBox *m_wpm;

    bool m_run = true;
    bool m_esm = true;
    bool m_cqRepeat = false;
    bool m_cqPending = false;
    int m_cqRepeatDelayMs = 2500;
    QString m_lastCall;
};

} // namespace qrl
