// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QSpinBox;

namespace qrl {

struct ClusterSettings
{
    QString callsign;
    bool clusterEnabled = false;
    QString clusterHost;
    int clusterPort = 7300;
    bool skimmerEnabled = true;
    QString skimmerHost = QStringLiteral("telnet.reversebeacon.net");
    int skimmerPort = 7000;
    int lifetimeMin = 30;

    static ClusterSettings load();
    void save() const;
};

class ClusterSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ClusterSettingsDialog(const ClusterSettings &settings, QWidget *parent = nullptr);
    ClusterSettings settings() const;

private:
    QLineEdit *m_call;
    QCheckBox *m_clusterOn;
    QLineEdit *m_clusterHost;
    QSpinBox *m_clusterPort;
    QCheckBox *m_skimmerOn;
    QLineEdit *m_skimmerHost;
    QSpinBox *m_skimmerPort;
    QSpinBox *m_lifetime;
};

} // namespace qrl
