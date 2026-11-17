// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMainWindow>

class QAction;
class QPlainTextEdit;

namespace qrl {

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
    void showConnectDialog();
    void appendLog(const QString &message);
    void updateActions();

    flex::FlexDiscovery *m_discovery;
    flex::FlexRadio *m_radio;
    QPlainTextEdit *m_log;
    QAction *m_connectAction = nullptr;
    QAction *m_disconnectAction = nullptr;
};

} // namespace qrl
