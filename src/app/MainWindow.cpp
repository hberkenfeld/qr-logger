// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/MainWindow.h"

#include "app/ConnectDialog.h"
#include "app/RadioPanel.h"
#include "radio/flex/FlexDiscovery.h"
#include "radio/flex/FlexRadio.h"

#include <QApplication>
#include <QDateTime>
#include <QDockWidget>
#include <QLabel>
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
{
    setWindowTitle(QStringLiteral("QR-Logger %1").arg(QApplication::applicationVersion()));
    resize(1100, 700);

    auto *placeholder = new QLabel(tr("QR-Logger – Eingabezeilen folgen in M2"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    setCentralWidget(placeholder);

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
    auto *radioMenu = menuBar()->addMenu(tr("&Radio"));
    m_connectAction = radioMenu->addAction(tr("&Verbinden …"), this, &MainWindow::showConnectDialog);
    m_disconnectAction = radioMenu->addAction(tr("&Trennen"), m_radio, &flex::FlexRadio::disconnectFromRadio);
    radioMenu->addSeparator();
    radioMenu->addAction(tr("&Beenden"), qApp, &QApplication::quit);
    auto *viewMenu = menuBar()->addMenu(tr("&Ansicht"));
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

    m_radio->setPreferredStation(QSettings().value(QStringLiteral("radio/boundStation")).toString());
    if (!m_discovery->start())
        appendLog(tr("Discovery nicht möglich (UDP-Port 4992 belegt?) – IP manuell eingeben."));

    statusBar()->showMessage(tr("Kein Radio verbunden"));
    updateActions();
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
