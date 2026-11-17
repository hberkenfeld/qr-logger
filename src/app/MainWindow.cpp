// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/MainWindow.h"

#include "app/ConnectDialog.h"
#include "app/ContestSetupDialog.h"
#include "app/MessagesDialog.h"
#include "app/RadioPanel.h"
#include "log/Cabrillo.h"
#include "log/LogDatabase.h"
#include "tx/CwKeyer.h"
#include "ui/ContestPanel.h"
#include "radio/flex/FlexDiscovery.h"
#include "radio/flex/FlexRadio.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QStandardPaths>
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
    , m_panel(new ContestPanel(m_logDb, m_radio, m_keyer, this))
    , m_score(new QLabel(this))
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
    auto *settingsMenu = menuBar()->addMenu(tr("&Einstellungen"));
    settingsMenu->addAction(tr("CW-&Nachrichten …"), this, &MainWindow::editMessages);
    auto *viewMenu = menuBar()->addMenu(tr("&Ansicht"));
    viewMenu->addAction(scoreDock->toggleViewAction());
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
    connect(m_keyer, &CwKeyer::sent, this, [this](const QString &text) { appendLog(tr("CW: %1").arg(text)); });
    connect(m_radio, &flex::FlexRadio::transmitChanged, this, [this] {
        static bool warned = false;
        if (!m_radio->cwBreakIn() && !warned) {
            warned = true;
            appendLog(tr("Hinweis: Break-In (QSK/Semi) ist am Radio aus – CWX tastet dann evtl. nicht."));
        }
    });

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
