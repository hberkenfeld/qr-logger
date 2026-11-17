// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/ClusterSettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace qrl {

ClusterSettings ClusterSettings::load()
{
    QSettings s;
    ClusterSettings c;
    c.callsign = s.value(QStringLiteral("cluster/callsign"), s.value(QStringLiteral("station/mycall"))).toString();
    c.clusterEnabled = s.value(QStringLiteral("cluster/clusterEnabled"), c.clusterEnabled).toBool();
    c.clusterHost = s.value(QStringLiteral("cluster/clusterHost"), c.clusterHost).toString();
    c.clusterPort = s.value(QStringLiteral("cluster/clusterPort"), c.clusterPort).toInt();
    c.skimmerEnabled = s.value(QStringLiteral("cluster/skimmerEnabled"), c.skimmerEnabled).toBool();
    c.skimmerHost = s.value(QStringLiteral("cluster/skimmerHost"), c.skimmerHost).toString();
    c.skimmerPort = s.value(QStringLiteral("cluster/skimmerPort"), c.skimmerPort).toInt();
    c.lifetimeMin = s.value(QStringLiteral("cluster/lifetimeMin"), c.lifetimeMin).toInt();
    return c;
}

void ClusterSettings::save() const
{
    QSettings s;
    s.setValue(QStringLiteral("cluster/callsign"), callsign);
    s.setValue(QStringLiteral("cluster/clusterEnabled"), clusterEnabled);
    s.setValue(QStringLiteral("cluster/clusterHost"), clusterHost);
    s.setValue(QStringLiteral("cluster/clusterPort"), clusterPort);
    s.setValue(QStringLiteral("cluster/skimmerEnabled"), skimmerEnabled);
    s.setValue(QStringLiteral("cluster/skimmerHost"), skimmerHost);
    s.setValue(QStringLiteral("cluster/skimmerPort"), skimmerPort);
    s.setValue(QStringLiteral("cluster/lifetimeMin"), lifetimeMin);
}

ClusterSettingsDialog::ClusterSettingsDialog(const ClusterSettings &c, QWidget *parent)
    : QDialog(parent)
    , m_call(new QLineEdit(c.callsign, this))
    , m_clusterOn(new QCheckBox(tr("verwenden"), this))
    , m_clusterHost(new QLineEdit(c.clusterHost, this))
    , m_clusterPort(new QSpinBox(this))
    , m_skimmerOn(new QCheckBox(tr("verwenden"), this))
    , m_skimmerHost(new QLineEdit(c.skimmerHost, this))
    , m_skimmerPort(new QSpinBox(this))
    , m_lifetime(new QSpinBox(this))
{
    setWindowTitle(tr("Cluster / Skimmer"));
    for (QSpinBox *port : {m_clusterPort, m_skimmerPort})
        port->setRange(1, 65535);
    m_clusterPort->setValue(c.clusterPort);
    m_skimmerPort->setValue(c.skimmerPort);
    m_clusterOn->setChecked(c.clusterEnabled);
    m_skimmerOn->setChecked(c.skimmerEnabled);
    m_clusterHost->setPlaceholderText(tr("Hostname des DX-Clusters"));
    m_lifetime->setRange(1, 240);
    m_lifetime->setSuffix(tr(" min"));
    m_lifetime->setValue(c.lifetimeMin);

    auto *cluster = new QGroupBox(tr("DX-Cluster"), this);
    auto *cf = new QFormLayout(cluster);
    cf->addRow(m_clusterOn);
    cf->addRow(tr("Host:"), m_clusterHost);
    cf->addRow(tr("Port:"), m_clusterPort);

    auto *skimmer = new QGroupBox(tr("Skimmer (RBN oder eigener Skimmer-Server)"), this);
    auto *sf = new QFormLayout(skimmer);
    sf->addRow(m_skimmerOn);
    sf->addRow(tr("Host:"), m_skimmerHost);
    sf->addRow(tr("Port:"), m_skimmerPort);

    auto *common = new QFormLayout;
    common->addRow(tr("Login-Rufzeichen:"), m_call);
    common->addRow(tr("Spot-Lebensdauer:"), m_lifetime);

    auto *note = new QLabel(tr("Hinweis: In Klassen ohne „Assisted“ sind Spots meist nicht erlaubt (z. B. CQ WW Single-Op unassisted)."), this);
    note->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(common);
    layout->addWidget(cluster);
    layout->addWidget(skimmer);
    layout->addWidget(note);
    layout->addWidget(buttons);
}

ClusterSettings ClusterSettingsDialog::settings() const
{
    ClusterSettings c;
    c.callsign = m_call->text().trimmed().toUpper();
    c.clusterEnabled = m_clusterOn->isChecked();
    c.clusterHost = m_clusterHost->text().trimmed();
    c.clusterPort = m_clusterPort->value();
    c.skimmerEnabled = m_skimmerOn->isChecked();
    c.skimmerHost = m_skimmerHost->text().trimmed();
    c.skimmerPort = m_skimmerPort->value();
    c.lifetimeMin = m_lifetime->value();
    return c;
}

} // namespace qrl
