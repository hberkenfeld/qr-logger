// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "radio/flex/FlexDiscovery.h"

#include <QDialog>

class QLineEdit;
class QListWidget;

namespace qrl {

// Lists radios found via discovery; alternatively takes a manual IP address.
class ConnectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectDialog(flex::FlexDiscovery *discovery, QWidget *parent = nullptr);

    QHostAddress address() const;
    quint16 port() const;

private:
    void rebuildList();

    flex::FlexDiscovery *m_discovery;
    QListWidget *m_list;
    QLineEdit *m_manualAddress;
};

} // namespace qrl
