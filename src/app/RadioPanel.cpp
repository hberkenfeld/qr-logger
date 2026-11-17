// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/RadioPanel.h"

#include "radio/flex/FlexRadio.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>

namespace qrl {

RadioPanel::RadioPanel(flex::FlexRadio *radio, QWidget *parent)
    : QWidget(parent)
    , m_radio(radio)
    , m_info(new QLabel(this))
    , m_clients(new QComboBox(this))
    , m_slices(new QTableWidget(0, 5, this))
{
    m_slices->setHorizontalHeaderLabels({tr("Slice"), tr("Frequenz (MHz)"), tr("Mode"), tr("TX"), tr("GUI-Client")});
    m_slices->horizontalHeader()->setStretchLastSection(true);
    m_slices->verticalHeader()->hide();
    m_slices->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_slices->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto *form = new QFormLayout;
    form->addRow(tr("Radio:"), m_info);
    form->addRow(tr("Binden an:"), m_clients);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_slices);

    connect(m_clients, &QComboBox::activated, this, [this](int index) {
        const QString id = m_clients->itemData(index).toString();
        if (!id.isEmpty() && id != m_radio->boundClientId())
            m_radio->bindToClient(id);
    });
    connect(m_radio, &flex::FlexRadio::stateChanged, this, &RadioPanel::updateInfo);
    connect(m_radio, &flex::FlexRadio::radioInfoChanged, this, &RadioPanel::updateInfo);
    connect(m_radio, &flex::FlexRadio::guiClientsChanged, this, &RadioPanel::updateClients);
    connect(m_radio, &flex::FlexRadio::boundClientChanged, this, [this] {
        updateClients();
        updateSlices();
        if (const auto *c = m_radio->boundClient())
            QSettings().setValue(QStringLiteral("radio/boundStation"), c->station);
    });
    connect(m_radio, &flex::FlexRadio::slicesChanged, this, &RadioPanel::updateSlices);

    updateInfo();
    updateClients();
}

void RadioPanel::updateInfo()
{
    using State = flex::FlexRadio::State;
    switch (m_radio->state()) {
    case State::Disconnected:
        m_info->setText(tr("nicht verbunden"));
        break;
    case State::Connecting:
        m_info->setText(tr("verbinde …"));
        break;
    case State::Connected:
        m_info->setText(QStringLiteral("%1 %2 %3").arg(m_radio->model(), m_radio->nickname(), m_radio->callsign()).simplified());
        break;
    }
}

void RadioPanel::updateClients()
{
    m_clients->clear();
    const auto clients = m_radio->guiClients();
    if (clients.isEmpty()) {
        m_clients->addItem(tr("kein GUI-Client gefunden"));
        m_clients->setEnabled(false);
        return;
    }
    m_clients->setEnabled(true);
    if (m_radio->boundClientId().isEmpty())
        m_clients->addItem(tr("– bitte wählen –"));
    for (const auto &c : clients) {
        m_clients->addItem(QStringLiteral("%1 – %2").arg(c.program, c.station), c.clientId);
        if (c.clientId == m_radio->boundClientId())
            m_clients->setCurrentIndex(m_clients->count() - 1);
    }
}

void RadioPanel::updateSlices()
{
    const auto slices = m_radio->slices();
    const auto *bound = m_radio->boundClient();
    m_slices->setRowCount(int(slices.size()));
    int row = 0;
    for (const auto &s : slices) {
        QString owner = QStringLiteral("0x%1").arg(s.clientHandle, 8, 16, QLatin1Char('0'));
        for (const auto &c : m_radio->guiClients()) {
            if (c.handle == s.clientHandle)
                owner = QStringLiteral("%1 – %2").arg(c.program, c.station);
        }
        const QStringList cells{
            s.letter.isEmpty() ? QString::number(s.index) : s.letter,
            QString::number(s.frequencyMHz, 'f', 3),
            s.mode,
            s.tx ? QStringLiteral("●") : QString(),
            owner,
        };
        for (int col = 0; col < cells.size(); ++col) {
            auto *item = new QTableWidgetItem(cells.at(col));
            if (bound && s.clientHandle != bound->handle)
                item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
            m_slices->setItem(row, col, item);
        }
        ++row;
    }
    m_slices->resizeColumnsToContents();
}

} // namespace qrl
