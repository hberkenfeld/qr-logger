// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QTableWidget;

namespace qrl {

namespace flex {
class FlexRadio;
}

// Shows radio state, GUI client binding and slices.
class RadioPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RadioPanel(flex::FlexRadio *radio, QWidget *parent = nullptr);

private:
    void updateInfo();
    void updateClients();
    void updateSlices();

    flex::FlexRadio *m_radio;
    QLabel *m_info;
    QComboBox *m_clients;
    QTableWidget *m_slices;
};

} // namespace qrl
