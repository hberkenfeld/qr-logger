// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMainWindow>

class QAction;
class QLabel;
class QPlainTextEdit;

namespace qrl {

class ContestPanel;
class CwKeyer;
class Sidetone;
class LogDatabase;
struct Score;

namespace flex {
class FlexDiscovery;
class FlexRadio;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void newLog();
    void openLog();
    bool openLogFile(const QString &path);
    void editContestSettings();
    void editMessages();
    void exportCabrillo();
    void showScore(const qrl::Score &score);
    void showConnectDialog();
    void appendLog(const QString &message);
    void updateActions();

    flex::FlexDiscovery *m_discovery;
    flex::FlexRadio *m_radio;
    QPlainTextEdit *m_log;
    LogDatabase *m_logDb;
    CwKeyer *m_keyer;
    Sidetone *m_sidetone;
    ContestPanel *m_panel;
    QLabel *m_score;
    QAction *m_connectAction = nullptr;
    QAction *m_disconnectAction = nullptr;
};

} // namespace qrl
