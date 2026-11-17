// SPDX-License-Identifier: GPL-3.0-or-later
//
// Keyboard mode (Alt-T / Alt-K): every typed character is sent immediately.
// Nothing is transmitted before the first character.
// Enter closes the window and lets the buffer finish, Esc closes and aborts.
#pragma once

#include <QDialog>
#include <functional>

class QLabel;
class QLineEdit;

namespace qrl {

class CwKeyer;

class FreeTextDialog : public QDialog
{
    Q_OBJECT

public:
    // canSend is asked before the first character (radio connected, CW mode, …).
    FreeTextDialog(CwKeyer *keyer, std::function<bool()> canSend, QWidget *parent = nullptr);

    QString sentText() const;

    static bool isSendable(QChar ch);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void reject() override;
    void done(int result) override;

private:
    CwKeyer *m_keyer;
    std::function<bool()> m_canSend;
    QLineEdit *m_edit;
    QLabel *m_hint;
    bool m_started = false;
    bool m_aborted = false;
};

} // namespace qrl
