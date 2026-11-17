// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/FreeTextDialog.h"

#include "tx/CwKeyer.h"
#include "tx/Macros.h"

#include <QFontDatabase>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace qrl {

FreeTextDialog::FreeTextDialog(CwKeyer *keyer, std::function<bool()> canSend, QWidget *parent)
    : QDialog(parent)
    , m_keyer(keyer)
    , m_canSend(std::move(canSend))
    , m_edit(new QLineEdit(this))
    , m_hint(new QLabel(this))
{
    setWindowTitle(tr("CW-Freitext"));
    setModal(true);
    resize(640, 120);

    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPointSizeF(f.pointSizeF() * 1.6);
    f.setBold(true);
    m_edit->setFont(f);
    m_edit->setPlaceholderText(tr("Tippen = senden …"));
    m_edit->installEventFilter(this);
    m_hint->setText(tr("Jedes Zeichen wird sofort gesendet · Enter: schließen (Rest wird gesendet) · Esc: schließen und abbrechen"));
    m_hint->setWordWrap(true);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_edit);
    layout->addWidget(m_hint);
    m_edit->setFocus();
}

QString FreeTextDialog::sentText() const
{
    return m_edit->text();
}

bool FreeTextDialog::isSendable(QChar ch)
{
    return ch == QLatin1Char(' ') || morsePattern(ch.toUpper()) != nullptr;
}

bool FreeTextDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_edit || event->type() != QEvent::KeyPress)
        return QDialog::eventFilter(watched, event);

    auto *ke = static_cast<QKeyEvent *>(event);
    switch (ke->key()) {
    case Qt::Key_Escape:
        reject();
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        accept();
        return true;
    case Qt::Key_PageUp:
        m_keyer->changeSpeed(+2);
        return true;
    case Qt::Key_PageDown:
        m_keyer->changeSpeed(-2);
        return true;
    default:
        break;
    }

    const QString text = ke->text();
    if (text.size() != 1 || (ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier)))
        return true; // no editing: what is typed has already been sent
    const QChar ch = text.at(0).toUpper();
    if (!isSendable(ch))
        return true;

    if (!m_started) {
        if (ch == QLatin1Char(' '))
            return true;
        if (!m_canSend())
            return true;
        m_started = true;
        m_keyer->setManualMode(true);
    }
    m_keyer->send(QString(ch), true);
    m_edit->setText(m_edit->text() + ch);
    return true;
}

void FreeTextDialog::reject()
{
    m_aborted = true;
    QDialog::reject();
}

void FreeTextDialog::done(int result)
{
    if (m_started) {
        if (m_aborted)
            m_keyer->stop();
        m_keyer->setManualMode(false);
    }
    QDialog::done(result);
}

} // namespace qrl
