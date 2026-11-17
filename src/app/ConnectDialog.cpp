// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/ConnectDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace qrl {

ConnectDialog::ConnectDialog(flex::FlexDiscovery *discovery, QWidget *parent)
    : QDialog(parent)
    , m_discovery(discovery)
    , m_list(new QListWidget(this))
    , m_manualAddress(new QLineEdit(this))
{
    setWindowTitle(tr("Mit FlexRadio verbinden"));
    resize(520, 320);

    m_manualAddress->setPlaceholderText(tr("z. B. 192.168.1.50"));
    m_manualAddress->setText(QSettings().value(QStringLiteral("radio/lastAddress")).toString());

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Verbinden"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *form = new QFormLayout;
    form->addRow(tr("Manuelle IP-Adresse:"), m_manualAddress);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Im Netz gefundene Radios:"), this));
    layout->addWidget(m_list);
    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0)
            m_manualAddress->setText(m_list->item(row)->data(Qt::UserRole).toString());
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    connect(m_discovery, &flex::FlexDiscovery::radioFound, this, &ConnectDialog::rebuildList);
    connect(m_discovery, &flex::FlexDiscovery::radioUpdated, this, &ConnectDialog::rebuildList);
    connect(m_discovery, &flex::FlexDiscovery::radioLost, this, &ConnectDialog::rebuildList);
    rebuildList();
}

QHostAddress ConnectDialog::address() const
{
    return QHostAddress(m_manualAddress->text().trimmed());
}

quint16 ConnectDialog::port() const
{
    const QString ip = address().toString();
    for (const auto &info : m_discovery->radios()) {
        if (info.address.toString() == ip)
            return info.port;
    }
    return flex::FlexDiscovery::Port;
}

void ConnectDialog::rebuildList()
{
    const QString current = m_manualAddress->text();
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    for (const auto &info : m_discovery->radios()) {
        QString text = QStringLiteral("%1  ·  SN %2  ·  v%3  ·  %4")
                           .arg(info.displayName(), info.serial, info.version, info.status);
        if (!info.guiClientPrograms.isEmpty())
            text += tr("  ·  GUI: %1").arg(info.guiClientPrograms.join(QStringLiteral(", ")));
        auto *item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole, info.address.toString());
        if (info.address.toString() == current)
            m_list->setCurrentItem(item);
    }
}

} // namespace qrl
