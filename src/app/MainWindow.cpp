// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/MainWindow.h"

#include <QApplication>
#include <QLabel>
#include <QStatusBar>

namespace qrl {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("QR-Logger %1").arg(QApplication::applicationVersion()));
    resize(1024, 640);

    auto *placeholder = new QLabel(tr("QR-Logger – Grundgerüst"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    setCentralWidget(placeholder);

    statusBar()->showMessage(tr("Kein Radio verbunden"));
}

} // namespace qrl
