// SPDX-License-Identifier: GPL-3.0-or-later
// QR-Logger – Contest logger for FlexRadio on Linux

#include <QApplication>

#include "app/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("QR-Logger"));
    QApplication::setOrganizationName(QStringLiteral("qr-logger"));
    QApplication::setApplicationVersion(QStringLiteral(QRL_VERSION));

    qrl::MainWindow window;
    window.show();
    return app.exec();
}
