// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/MainWindow.h"

#include "app/ClusterSettingsDialog.h"
#include "app/ConnectDialog.h"
#include "app/ContestSetupDialog.h"
#include "app/MessagesDialog.h"
#include "app/RadioPanel.h"
#include "audio/Sidetone.h"
#include "log/Cabrillo.h"
#include "log/LogDatabase.h"
#include "tx/CwKeyer.h"
#include "contest/Bands.h"
#include "net/ClusterClient.h"
#include "ui/BandmapPanel.h"
#include "ui/ContestPanel.h"
#include "radio/flex/FlexDiscovery.h"
#include "radio/flex/FlexRadio.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QShortcut>
#include <QStandardPaths>
#include <QTimer>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QSettings>
#include <QStatusBar>

namespace qrl {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_discovery(new flex::FlexDiscovery(this))
    , m_radio(new flex::FlexRadio(this))
    , m_log(new QPlainTextEdit(this))
    , m_logDb(new LogDatabase(this))
    , m_keyer(new CwKeyer(m_radio, this))
    , m_sidetone(new Sidetone(this))
    , m_panel(new ContestPanel(m_logDb, m_radio, m_keyer, this))
    , m_score(new QLabel(this))
    , m_bandmap(nullptr)
    , m_cluster(new ClusterClient(QStringLiteral("Cluster"), this))
    , m_skimmer(new ClusterClient(QStringLiteral("Skimmer"), this))
{
    setWindowTitle(QStringLiteral("QR-Logger %1").arg(QApplication::applicationVersion()));
    resize(1280, 800);
    setCentralWidget(m_panel);

    // Score
    QFont scoreFont = m_score->font();
    scoreFont.setPointSizeF(scoreFont.pointSizeF() * 1.2);
    m_score->setFont(scoreFont);
    m_score->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_score->setMargin(6);
    m_score->setWordWrap(true);
    auto *scoreDock = new QDockWidget(tr("Punkte"), this);
    scoreDock->setObjectName(QStringLiteral("scoreDock"));
    scoreDock->setWidget(m_score);
    addDockWidget(Qt::RightDockWidgetArea, scoreDock);

    // Radio panel
    auto *radioDock = new QDockWidget(tr("Radio"), this);
    radioDock->setObjectName(QStringLiteral("radioDock"));
    radioDock->setWidget(new RadioPanel(m_radio, radioDock));
    addDockWidget(Qt::RightDockWidgetArea, radioDock);

    // Bandmap
    m_bandmap = new BandmapPanel(
        [this](const QString &call, const QString &band) {
            const Contest *contest = m_panel->contest();
            Qso candidate;
            candidate.call = call;
            candidate.band = band;
            candidate.time = QDateTime::currentDateTimeUtc();
            if (contest && !contest->dupeReason(candidate, m_panel->qsos()).isEmpty())
                return BandmapPanel::Status::Dupe;
            for (const Qso &q : m_panel->qsos()) {
                if (q.call == call)
                    return BandmapPanel::Status::NewBand;
            }
            return BandmapPanel::Status::New;
        },
        [this] {
            const auto slice = m_radio->txSlice();
            return slice ? slice->frequencyMHz * 1000.0 : 0.0;
        },
        this);
    auto *bandmapDock = new QDockWidget(tr("Spots"), this);
    bandmapDock->setObjectName(QStringLiteral("bandmapDock"));
    bandmapDock->setWidget(m_bandmap);
    addDockWidget(Qt::LeftDockWidgetArea, bandmapDock);

    // Event log
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(2000);
    auto *logDock = new QDockWidget(tr("Ereignisse"), this);
    logDock->setObjectName(QStringLiteral("logDock"));
    logDock->setWidget(m_log);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);

    // Menu
    auto *fileMenu = menuBar()->addMenu(tr("&Datei"));
    fileMenu->addAction(tr("&Neues Log …"), this, &MainWindow::newLog);
    fileMenu->addAction(tr("Log ö&ffnen …"), this, &MainWindow::openLog);
    fileMenu->addAction(tr("Contest-&Einstellungen …"), this, &MainWindow::editContestSettings);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Cabrillo exportieren …"), this, &MainWindow::exportCabrillo);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Beenden"), qApp, &QApplication::quit);

    auto *radioMenu = menuBar()->addMenu(tr("&Radio"));
    m_connectAction = radioMenu->addAction(tr("&Verbinden …"), this, &MainWindow::showConnectDialog);
    m_disconnectAction = radioMenu->addAction(tr("&Trennen"), m_radio, &flex::FlexRadio::disconnectFromRadio);
    auto *clusterMenu = menuBar()->addMenu(tr("&Cluster"));
    clusterMenu->addAction(tr("&Verbinden"), this, &MainWindow::connectCluster);
    clusterMenu->addAction(tr("&Trennen"), this, &MainWindow::disconnectCluster);
    clusterMenu->addAction(tr("&Einstellungen …"), this, &MainWindow::editClusterSettings);

    auto *settingsMenu = menuBar()->addMenu(tr("&Einstellungen"));
    settingsMenu->addAction(tr("CW-&Nachrichten …"), this, &MainWindow::editMessages);
    settingsMenu->addSeparator();
    auto *holdPtt = settingsMenu->addAction(tr("CW: &PTT während Nachricht halten (xmit 1/0)"));
    holdPtt->setCheckable(true);
    holdPtt->setChecked(QSettings().value(QStringLiteral("cw/holdPtt"), false).toBool());
    m_keyer->setHoldPtt(holdPtt->isChecked());
    connect(holdPtt, &QAction::toggled, this, [this](bool on) {
        m_keyer->setHoldPtt(on);
        QSettings().setValue(QStringLiteral("cw/holdPtt"), on);
        appendLog(on ? tr("CW: PTT wird während der Nachricht gehalten") : tr("CW: Tastung über Break-In"));
    });
    auto *sidetone = settingsMenu->addAction(tr("CW: &Mithörton im PC"));
    sidetone->setCheckable(true);
    sidetone->setChecked(QSettings().value(QStringLiteral("cw/sidetone"), true).toBool());
    m_sidetone->setEnabled(sidetone->isChecked());
    m_sidetone->setVolume(QSettings().value(QStringLiteral("cw/sidetoneVolume"), 0.3).toDouble());
    connect(sidetone, &QAction::toggled, this, [this](bool on) {
        m_sidetone->setEnabled(on);
        QSettings().setValue(QStringLiteral("cw/sidetone"), on);
    });
    settingsMenu->addAction(tr("CW: Mithörton-&Lautstärke …"), this, [this] {
        bool ok = false;
        const int percent = QInputDialog::getInt(this, tr("Mithörton"), tr("Lautstärke (%):"),
                                                 int(QSettings().value(QStringLiteral("cw/sidetoneVolume"), 0.3).toDouble() * 100), 0, 100, 5, &ok);
        if (!ok)
            return;
        m_sidetone->setVolume(percent / 100.0);
        QSettings().setValue(QStringLiteral("cw/sidetoneVolume"), percent / 100.0);
    });
    auto *interlockLog = settingsMenu->addAction(tr("&Diagnose: TX-Interlock protokollieren"));
    interlockLog->setCheckable(true);
    connect(interlockLog, &QAction::toggled, m_radio, &flex::FlexRadio::setLogInterlock);
    auto *viewMenu = menuBar()->addMenu(tr("&Ansicht"));
    viewMenu->addAction(scoreDock->toggleViewAction());
    viewMenu->addAction(bandmapDock->toggleViewAction());
    viewMenu->addAction(radioDock->toggleViewAction());
    viewMenu->addAction(logDock->toggleViewAction());

    // Wiring
    connect(m_radio, &flex::FlexRadio::logMessage, this, &MainWindow::appendLog);
    connect(m_radio, &flex::FlexRadio::errorOccurred, this,
            [this](const QString &m) { appendLog(tr("Fehler: %1").arg(m)); });
    connect(m_radio, &flex::FlexRadio::stateChanged, this, [this](flex::FlexRadio::State s) {
        updateActions();
        switch (s) {
        case flex::FlexRadio::State::Disconnected:
            statusBar()->showMessage(tr("Kein Radio verbunden"));
            break;
        case flex::FlexRadio::State::Connecting:
            statusBar()->showMessage(tr("Verbinde …"));
            break;
        case flex::FlexRadio::State::Connected:
            statusBar()->showMessage(tr("Verbunden mit %1").arg(m_radio->connection().peerAddress().toString()));
            break;
        }
    });
    connect(m_radio, &flex::FlexRadio::boundClientChanged, this, [this](const QString &id) {
        if (id.isEmpty() && m_radio->state() == flex::FlexRadio::State::Connected)
            appendLog(tr("Nicht an einen GUI-Client gebunden"));
    });
    connect(m_discovery, &flex::FlexDiscovery::radioFound, this, [this](const flex::RadioInfo &info) {
        appendLog(tr("Radio gefunden: %1").arg(info.displayName()));
    });

    connect(m_panel, &ContestPanel::statusMessage, this, [this](const QString &m) {
        statusBar()->showMessage(m, 8000);
        appendLog(m);
    });
    connect(m_panel, &ContestPanel::scoreChanged, this, &MainWindow::showScore);
    connect(m_keyer, &CwKeyer::diagnostic, this, &MainWindow::appendLog);
    connect(m_keyer, &CwKeyer::sent, this, [this](const QString &text, bool continuous) {
        m_sidetone->setPitch(m_radio->cwPitch());
        m_sidetone->play(text, m_keyer->speed(), continuous);
    });
    connect(m_keyer, &CwKeyer::stopped, m_sidetone, &Sidetone::stop);
    connect(m_keyer, &CwKeyer::sent, this, [this](const QString &text, bool continuous) {
        if (!continuous)
            appendLog(tr("CW: %1").arg(text));
    });
    connect(m_radio, &flex::FlexRadio::transmitChanged, this, [this] {
        static bool warned = false;
        if (!m_radio->cwBreakIn() && !warned) {
            warned = true;
            appendLog(tr("Hinweis: Break-In (QSK/Semi) ist am Radio aus – CWX tastet dann evtl. nicht."));
        }
    });

    // Cluster / skimmer
    for (ClusterClient *client : {m_cluster, m_skimmer}) {
        connect(client, &ClusterClient::spotReceived, m_bandmap, &BandmapPanel::addSpot);
        connect(client, &ClusterClient::stateChanged, this, &MainWindow::updateClusterStatus);
        connect(client, &ClusterClient::errorOccurred, this, [this, client](const QString &m) {
            appendLog(tr("%1: %2").arg(client->name(), m));
        });
    }
    connect(m_cluster, &ClusterClient::lineReceived, this, [this](const QString &line) {
        appendLog(tr("Cluster: %1").arg(line));
    });
    connect(m_bandmap, &BandmapPanel::spotSelected, this, [this](const Spot &spot) {
        const auto slice = m_radio->txSlice();
        if (!slice) {
            statusBar()->showMessage(tr("Kein TX-Slice – Spot nicht übernommen"), 5000);
            return;
        }
        m_radio->tuneSlice(slice->index, spot.freqKhz);
        m_panel->takeSpot(spot.call);
        statusBar()->showMessage(tr("%1 auf %2 kHz").arg(spot.call).arg(spot.freqKhz, 0, 'f', 1), 5000);
    });
    // Spot jumping from anywhere in the window: Shift-Up/Down (and Ctrl-Up/Down as in DXLog)
    for (const auto &[seq, up] : {std::pair{QKeySequence(Qt::SHIFT | Qt::Key_Up), true},
                                  std::pair{QKeySequence(Qt::SHIFT | Qt::Key_Down), false},
                                  std::pair{QKeySequence(Qt::CTRL | Qt::Key_Up), true},
                                  std::pair{QKeySequence(Qt::CTRL | Qt::Key_Down), false}}) {
        auto *shortcut = new QShortcut(seq, this);
        shortcut->setContext(Qt::ApplicationShortcut);
        connect(shortcut, &QShortcut::activated, m_bandmap, [this, up = up] { m_bandmap->tuneNext(up); });
    }
    connect(m_radio, &flex::FlexRadio::slicesChanged, m_bandmap, &BandmapPanel::refresh);
    m_bandmap->setLifetimeMinutes(ClusterSettings::load().lifetimeMin);
    updateClusterStatus();
    if (QSettings().value(QStringLiteral("cluster/autoConnect"), false).toBool())
        QTimer::singleShot(0, this, &MainWindow::connectCluster);

    const QString lastLog = QSettings().value(QStringLiteral("log/last")).toString();
    if (!lastLog.isEmpty() && QFile::exists(lastLog))
        openLogFile(lastLog);
    else
        showScore(Score{});

    m_radio->setPreferredStation(QSettings().value(QStringLiteral("radio/boundStation")).toString());
    if (!m_discovery->start())
        appendLog(tr("Discovery nicht möglich (UDP-Port 4992 belegt?) – IP manuell eingeben."));

    statusBar()->showMessage(tr("Kein Radio verbunden"));
    updateActions();
}

void MainWindow::newLog()
{
    StationSettings defaults;
    QSettings settings;
    defaults.myCall = settings.value(QStringLiteral("station/mycall")).toString();
    defaults.club = settings.value(QStringLiteral("station/club")).toString();
    defaults.email = settings.value(QStringLiteral("station/email")).toString();
    defaults.name = settings.value(QStringLiteral("station/name")).toString();
    defaults.cqZone = settings.value(QStringLiteral("station/cqzone"), 14).toInt();

    ContestSetupDialog dialog(defaults, true, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const StationSettings st = dialog.settings();
    settings.setValue(QStringLiteral("station/mycall"), st.myCall);
    settings.setValue(QStringLiteral("station/club"), st.club);
    settings.setValue(QStringLiteral("station/email"), st.email);
    settings.setValue(QStringLiteral("station/name"), st.name);
    settings.setValue(QStringLiteral("station/cqzone"), st.cqZone);

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/logs");
    QDir().mkpath(dir);
    QString base = QStringLiteral("%1/%2_%3_%4").arg(dir, st.contestId, QString(st.myCall).replace(QLatin1Char('/'), QLatin1Char('-')),
                                                     QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd")));
    QString path = base + QStringLiteral(".qrl");
    for (int i = 2; QFile::exists(path); ++i)
        path = QStringLiteral("%1_%2.qrl").arg(base).arg(i);

    QString error;
    if (!m_logDb->open(path, &error)) {
        QMessageBox::critical(this, tr("Log"), tr("Log konnte nicht angelegt werden:\n%1").arg(error));
        return;
    }
    m_logDb->setStation(st);
    settings.setValue(QStringLiteral("log/last"), path);
    m_panel->reload();
    setWindowTitle(QStringLiteral("QR-Logger %1 – %2").arg(QApplication::applicationVersion(), QFileInfo(path).fileName()));
    appendLog(tr("Neues Log: %1").arg(path));
}

void MainWindow::openLog()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/logs");
    const QString path = QFileDialog::getOpenFileName(this, tr("Log öffnen"), dir, tr("QR-Logger-Logs (*.qrl)"));
    if (!path.isEmpty())
        openLogFile(path);
}

bool MainWindow::openLogFile(const QString &path)
{
    QString error;
    if (!m_logDb->open(path, &error)) {
        appendLog(tr("Log konnte nicht geöffnet werden: %1").arg(error));
        return false;
    }
    QSettings().setValue(QStringLiteral("log/last"), path);
    m_panel->reload();
    m_bandmap->setModeFilter(m_logDb->station().categoryMode == QLatin1String("MIXED") ? QString() : m_logDb->station().categoryMode);
    setWindowTitle(QStringLiteral("QR-Logger %1 – %2").arg(QApplication::applicationVersion(), QFileInfo(path).fileName()));
    appendLog(tr("Log geöffnet: %1 (%2 QSOs)").arg(path).arg(m_logDb->count()));
    return true;
}

void MainWindow::editContestSettings()
{
    if (!m_logDb->isOpen())
        return newLog();
    ContestSetupDialog dialog(m_logDb->station(), false, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_logDb->setStation(dialog.settings());
    m_panel->reload();
}

void MainWindow::editMessages()
{
    const Contest *contest = m_panel->contest();
    if (!contest) {
        QMessageBox::information(this, tr("CW-Nachrichten"), tr("Bitte zuerst ein Log anlegen oder öffnen."));
        return;
    }
    MessagesDialog dialog(m_panel->messages(true), m_panel->messages(false), contest->defaultMessages(true),
                          contest->defaultMessages(false), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_panel->setMessages(true, dialog.run());
    m_panel->setMessages(false, dialog.sp());
}

void MainWindow::exportCabrillo()
{
    const Contest *contest = m_panel->contest();
    if (!contest || !m_logDb->isOpen())
        return;
    const StationSettings st = m_logDb->station();
    const QString suggestion = QDir::home().filePath(QStringLiteral("%1_%2.log").arg(st.contestId, QString(st.myCall).replace(QLatin1Char('/'), QLatin1Char('-'))));
    const QString path = QFileDialog::getSaveFileName(this, tr("Cabrillo exportieren"), suggestion, tr("Cabrillo (*.log *.cbr)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, tr("Cabrillo"), file.errorString());
        return;
    }
    file.write(qrl::exportCabrillo(*contest, st, m_logDb->qsos()).toUtf8());
    appendLog(tr("Cabrillo geschrieben: %1").arg(path));
}

void MainWindow::showScore(const Score &s)
{
    if (!m_panel->contest()) {
        m_score->setText(tr("Kein Log geöffnet.<br>Datei → Neues Log …"));
        return;
    }
    m_score->setText(tr("<b>%1</b><br>QSOs: <b>%2</b> (Dupes %3)<br>Punkte: <b>%4</b><br>Multis: <b>%5</b><br>"
                        "Score: <b style='font-size:x-large'>%6</b><br><small>%7</small>")
                         .arg(m_panel->contest()->name())
                         .arg(s.qsos).arg(s.dupes).arg(s.points).arg(s.multipliers).arg(s.total)
                         .arg(s.details.toHtmlEscaped()));
}

void MainWindow::connectCluster()
{
    const ClusterSettings c = ClusterSettings::load();
    if (c.callsign.size() < 3) {
        editClusterSettings();
        return;
    }
    if (c.clusterEnabled && !c.clusterHost.isEmpty())
        m_cluster->connectTo(c.clusterHost, quint16(c.clusterPort), c.callsign);
    if (c.skimmerEnabled && !c.skimmerHost.isEmpty())
        m_skimmer->connectTo(c.skimmerHost, quint16(c.skimmerPort), c.callsign);
    QSettings().setValue(QStringLiteral("cluster/autoConnect"), true);
}

void MainWindow::disconnectCluster()
{
    m_cluster->disconnectFrom();
    m_skimmer->disconnectFrom();
    QSettings().setValue(QStringLiteral("cluster/autoConnect"), false);
}

void MainWindow::editClusterSettings()
{
    ClusterSettingsDialog dialog(ClusterSettings::load(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const ClusterSettings c = dialog.settings();
    c.save();
    m_bandmap->setLifetimeMinutes(c.lifetimeMin);
    if (m_cluster->state() != ClusterClient::State::Disconnected || m_skimmer->state() != ClusterClient::State::Disconnected) {
        disconnectCluster();
        connectCluster();
    }
}

void MainWindow::updateClusterStatus()
{
    auto text = [](const ClusterClient *c) {
        switch (c->state()) {
        case ClusterClient::State::Connected:
            return QStringLiteral("%1: <b style='color:#080'>%2</b>").arg(c->name(), c->endpoint());
        case ClusterClient::State::Connecting:
            return QStringLiteral("%1: verbinde %2 …").arg(c->name(), c->endpoint());
        case ClusterClient::State::Disconnected:
            break;
        }
        return QStringLiteral("%1: –").arg(c->name());
    };
    m_bandmap->setConnectionStatus(text(m_cluster) + QStringLiteral(" · ") + text(m_skimmer));
}

void MainWindow::showConnectDialog()
{
    ConnectDialog dialog(m_discovery, this);
    if (dialog.exec() != QDialog::Accepted || dialog.address().isNull())
        return;
    QSettings().setValue(QStringLiteral("radio/lastAddress"), dialog.address().toString());
    appendLog(tr("Verbinde mit %1 …").arg(dialog.address().toString()));
    m_radio->connectToRadio(dialog.address(), dialog.port());
}

void MainWindow::appendLog(const QString &message)
{
    m_log->appendPlainText(QStringLiteral("%1  %2")
                               .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("HH:mm:ss")), message));
}

void MainWindow::updateActions()
{
    const bool disconnected = m_radio->state() == flex::FlexRadio::State::Disconnected;
    m_connectAction->setEnabled(disconnected);
    m_disconnectAction->setEnabled(!disconnected);
}

} // namespace qrl
