// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "contest/Qso.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace qrl {

// Contest selection and station/category data for a log.
class ContestSetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ContestSetupDialog(const StationSettings &initial, bool newLog, QWidget *parent = nullptr);

    StationSettings settings() const;

private:
    void updateCategories();

    QComboBox *m_contest;
    QLineEdit *m_myCall;
    QComboBox *m_operator;
    QComboBox *m_band;
    QComboBox *m_power;
    QComboBox *m_mode;
    QSpinBox *m_zone;
    QLineEdit *m_club;
    QLineEdit *m_email;
    QLineEdit *m_name;
};

} // namespace qrl
