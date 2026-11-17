// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/MessagesDialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace qrl {

namespace {
const QStringList Keys{QStringLiteral("F1"), QStringLiteral("F2"), QStringLiteral("F3"), QStringLiteral("F4"),
                       QStringLiteral("F5"), QStringLiteral("F6"), QStringLiteral("F7"), QStringLiteral("F8"),
                       QStringLiteral("INS"), QStringLiteral("PLUS")};
}

MessagesDialog::MessagesDialog(const MessageSet &run, const MessageSet &sp, const MessageSet &defaultRun,
                               const MessageSet &defaultSp, QWidget *parent)
    : QDialog(parent)
    , m_table(new QTableWidget(int(Keys.size()), 2, this))
{
    setWindowTitle(tr("CW-Nachrichten"));
    resize(720, 420);
    m_table->setHorizontalHeaderLabels({tr("Run"), tr("S&P")});
    m_table->setVerticalHeaderLabels({QStringLiteral("F1"), QStringLiteral("F2"), QStringLiteral("F3"),
                                      QStringLiteral("F4"), QStringLiteral("F5"), QStringLiteral("F6"),
                                      QStringLiteral("F7"), QStringLiteral("F8"), QStringLiteral("Insert"),
                                      QStringLiteral("+")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    fill(run, sp);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                             | QDialogButtonBox::RestoreDefaults, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
            [this, defaultRun, defaultSp] { fill(defaultRun, defaultSp); });

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Variablen: $MYCALL $CALL $LASTCALL $RST $SERIAL $CQZONE $EXCHANGE"), this));
    layout->addWidget(m_table);
    layout->addWidget(buttons);
}

void MessagesDialog::fill(const MessageSet &run, const MessageSet &sp)
{
    for (int row = 0; row < Keys.size(); ++row) {
        m_table->setItem(row, 0, new QTableWidgetItem(run.value(Keys.at(row))));
        m_table->setItem(row, 1, new QTableWidgetItem(sp.value(Keys.at(row))));
    }
}

MessageSet MessagesDialog::column(int col) const
{
    MessageSet set;
    for (int row = 0; row < Keys.size(); ++row)
        set.insert(Keys.at(row), m_table->item(row, col) ? m_table->item(row, col)->text().trimmed() : QString());
    return set;
}

} // namespace qrl
