// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/ContestSetupDialog.h"

#include "contest/Contest.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSpinBox>

namespace qrl {

ContestSetupDialog::ContestSetupDialog(const StationSettings &initial, bool newLog, QWidget *parent)
    : QDialog(parent)
    , m_contest(new QComboBox(this))
    , m_myCall(new QLineEdit(this))
    , m_operator(new QComboBox(this))
    , m_band(new QComboBox(this))
    , m_power(new QComboBox(this))
    , m_mode(new QComboBox(this))
    , m_zone(new QSpinBox(this))
    , m_club(new QLineEdit(this))
    , m_email(new QLineEdit(this))
    , m_name(new QLineEdit(this))
{
    setWindowTitle(newLog ? tr("Neues Contest-Log") : tr("Contest-Einstellungen"));

    for (const QString &id : availableContests()) {
        const auto c = createContest(id);
        m_contest->addItem(c->name(), id);
    }
    m_contest->setCurrentIndex(qMax(0, m_contest->findData(initial.contestId)));
    m_contest->setEnabled(newLog);

    m_myCall->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[A-Za-z0-9/]{3,16}")), m_myCall));
    m_myCall->setText(initial.myCall);
    m_operator->addItems({QStringLiteral("SINGLE-OP"), QStringLiteral("MULTI-OP"), QStringLiteral("CHECKLOG")});
    m_operator->setCurrentText(initial.categoryOperator);
    m_mode->addItems({QStringLiteral("CW"), QStringLiteral("SSB"), QStringLiteral("RTTY"), QStringLiteral("MIXED")});
    m_mode->setCurrentText(initial.categoryMode);
    m_zone->setRange(1, 40);
    m_zone->setValue(initial.cqZone);
    m_club->setText(initial.club);
    m_email->setText(initial.email);
    m_name->setText(initial.name);

    auto *form = new QFormLayout;
    form->addRow(tr("Contest:"), m_contest);
    form->addRow(tr("Eigenes Rufzeichen:"), m_myCall);
    form->addRow(tr("CATEGORY-OPERATOR:"), m_operator);
    form->addRow(tr("CATEGORY-BAND:"), m_band);
    form->addRow(tr("CATEGORY-POWER:"), m_power);
    form->addRow(tr("CATEGORY-MODE:"), m_mode);
    form->addRow(tr("CQ-Zone:"), m_zone);
    form->addRow(tr("CLUB:"), m_club);
    form->addRow(tr("EMAIL:"), m_email);
    form->addRow(tr("NAME:"), m_name);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (m_myCall->text().size() >= 3)
            accept();
        else
            m_myCall->setFocus();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    form->addRow(buttons);
    setLayout(form);

    connect(m_contest, &QComboBox::currentIndexChanged, this, &ContestSetupDialog::updateCategories);
    updateCategories();
    m_band->setCurrentText(initial.categoryBand);
    m_power->setCurrentText(initial.categoryPower);
}

void ContestSetupDialog::updateCategories()
{
    const auto contest = createContest(m_contest->currentData().toString());
    if (!contest)
        return;
    const QString band = m_band->currentText();
    const QString power = m_power->currentText();
    m_band->clear();
    m_band->addItems(contest->categoryBands());
    m_power->clear();
    m_power->addItems(contest->categoryPowers());
    if (!band.isEmpty())
        m_band->setCurrentText(band);
    if (!power.isEmpty())
        m_power->setCurrentText(power);
}

StationSettings ContestSetupDialog::settings() const
{
    StationSettings s;
    s.contestId = m_contest->currentData().toString();
    s.myCall = m_myCall->text().trimmed().toUpper();
    s.categoryOperator = m_operator->currentText();
    s.categoryBand = m_band->currentText();
    s.categoryPower = m_power->currentText();
    s.categoryMode = m_mode->currentText();
    s.cqZone = m_zone->value();
    s.club = m_club->text().trimmed();
    s.email = m_email->text().trimmed();
    s.name = m_name->text().trimmed();
    return s;
}

} // namespace qrl
