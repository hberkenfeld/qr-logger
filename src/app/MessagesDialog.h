// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "contest/Contest.h"

#include <QDialog>

class QTableWidget;

namespace qrl {

// Edits the CW messages (F1–F8, Insert, +) for Run and S&P.
class MessagesDialog : public QDialog
{
    Q_OBJECT

public:
    MessagesDialog(const MessageSet &run, const MessageSet &sp, const MessageSet &defaultRun,
                   const MessageSet &defaultSp, QWidget *parent = nullptr);

    MessageSet run() const { return column(0); }
    MessageSet sp() const { return column(1); }

private:
    MessageSet column(int col) const;
    void fill(const MessageSet &run, const MessageSet &sp);

    QTableWidget *m_table;
};

} // namespace qrl
