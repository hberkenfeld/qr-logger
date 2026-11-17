// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/ContestPanel.h"

#include "contest/Bands.h"
#include "log/LogDatabase.h"
#include "radio/flex/FlexRadio.h"
#include "tx/CwKeyer.h"
#include "tx/Macros.h"
#include "ui/FreeTextDialog.h"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpressionValidator>
#include <QSettings>
#include <QSqlTableModel>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

namespace qrl {

namespace {

const QStringList MessageKeys{QStringLiteral("F1"), QStringLiteral("F2"), QStringLiteral("F3"),
                              QStringLiteral("F4"), QStringLiteral("F5"), QStringLiteral("F6"),
                              QStringLiteral("F7"), QStringLiteral("F8"), QStringLiteral("INS"),
                              QStringLiteral("PLUS")};

QString settingsKey(const QString &contestId, bool run, const QString &key)
{
    return QStringLiteral("messages/%1/%2/%3").arg(contestId, run ? QStringLiteral("run") : QStringLiteral("sp"), key);
}

QLineEdit *makeField(QWidget *parent, int width, const QString &placeholder)
{
    auto *edit = new QLineEdit(parent);
    QFont f = edit->font();
    f.setPointSizeF(f.pointSizeF() * 1.6);
    f.setBold(true);
    edit->setFont(f);
    edit->setMinimumWidth(width);
    edit->setPlaceholderText(placeholder);
    return edit;
}

} // namespace

ContestPanel::ContestPanel(LogDatabase *log, flex::FlexRadio *radio, CwKeyer *keyer, QWidget *parent)
    : QWidget(parent)
    , m_log(log)
    , m_radio(radio)
    , m_keyer(keyer)
    , m_view(new QTableView(this))
    , m_call(makeField(this, 220, tr("Rufzeichen")))
    , m_rst(makeField(this, 70, tr("RST")))
    , m_exch(makeField(this, 120, tr("Exch")))
    , m_sentLabel(new QLabel(this))
    , m_infoLabel(new QLabel(this))
    , m_modeLabel(new QLabel(this))
    , m_txLabel(new QLabel(this))
{
    m_call->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[A-Za-z0-9/]{0,16}")), m_call));
    m_rst->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[1-5][1-9][1-9]?")), m_rst));
    m_rst->setText(QStringLiteral("599"));

    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->verticalHeader()->setDefaultSectionSize(m_view->fontMetrics().height() + 6);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->installEventFilter(this);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        if (!m_view->indexAt(pos).isValid())
            return;
        QMenu menu(this);
        menu.addAction(tr("QSO löschen …"), this, &ContestPanel::deleteSelectedQso);
        menu.exec(m_view->viewport()->mapToGlobal(pos));
    });

    QFont big = m_sentLabel->font();
    big.setPointSizeF(big.pointSizeF() * 1.3);
    m_sentLabel->setFont(big);
    m_modeLabel->setFont(big);
    m_infoLabel->setFont(big);
    m_infoLabel->setMinimumHeight(m_infoLabel->fontMetrics().height() + 4);

    QFont txFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    txFont.setPointSizeF(big.pointSizeF() * 1.2);
    m_txLabel->setFont(txFont);
    m_txLabel->setTextFormat(Qt::RichText);
    m_txLabel->setMinimumHeight(m_txLabel->fontMetrics().height() + 8);
    m_txLabel->setStyleSheet(QStringLiteral("QLabel { padding: 2px 6px; border: 1px solid palette(mid); }"));

    auto *entry = new QHBoxLayout;
    entry->addWidget(m_call, 3);
    entry->addWidget(m_rst, 1);
    entry->addWidget(m_exch, 2);
    entry->addWidget(m_sentLabel);
    entry->addStretch();
    entry->addWidget(m_modeLabel);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_view, 1);
    layout->addWidget(m_txLabel);
    layout->addLayout(entry);
    layout->addWidget(m_infoLabel);

    for (QLineEdit *field : {m_call, m_rst, m_exch})
        field->installEventFilter(this);

    connect(m_call, &QLineEdit::textEdited, this, [this](const QString &text) {
        const QString upper = text.toUpper();
        if (upper != text) {
            const int pos = m_call->cursorPosition();
            m_call->setText(upper);
            m_call->setCursorPosition(pos);
        }
        m_cqPending = false;
        updateInfo();
    });
    connect(m_exch, &QLineEdit::textEdited, this, &ContestPanel::updateInfo);

    connect(m_keyer, &CwKeyer::finished, this, [this] {
        if (!(m_cqRepeat && m_cqPending))
            return;
        QTimer::singleShot(m_cqRepeatDelayMs, this, [this] {
            if (m_cqRepeat && m_cqPending && m_call->text().isEmpty() && !m_keyer->isSending())
                sendMessage(QStringLiteral("F1"));
        });
    });
    connect(m_keyer, &CwKeyer::progress, this, [this](const QString &text, int sentChars) {
        if (text.isEmpty()) {
            m_txLabel->setText(QStringLiteral("<span style='color:gray'>TX –</span>"));
            return;
        }
        const QString done = text.left(sentChars).toHtmlEscaped();
        const QString current = text.mid(sentChars, 1).toHtmlEscaped();
        const QString rest = text.mid(sentChars + 1).toHtmlEscaped();
        m_txLabel->setText(QStringLiteral("<b style='color:#c00'>TX</b>&nbsp;&nbsp;<span style='color:gray'>%1</span>"
                                          "<b style='background:#c00;color:white'>%2</b><b>%3</b>")
                               .arg(done, current.isEmpty() ? QStringLiteral("&nbsp;") : current, rest));
    });
    connect(m_keyer, &CwKeyer::speedChanged, this, &ContestPanel::updateModeLabel);
    connect(m_keyer, &CwKeyer::sendingChanged, this, &ContestPanel::updateModeLabel);
    connect(m_log, &LogDatabase::changed, this, &ContestPanel::updateScore);

    auto *clock = new QTimer(this);
    clock->setInterval(1000);
    connect(clock, &QTimer::timeout, this, &ContestPanel::updateInfo);
    clock->start();

    m_txLabel->setText(QStringLiteral("<span style='color:gray'>TX –</span>"));
    reload();
}

ContestPanel::~ContestPanel() = default;

void ContestPanel::reload()
{
    delete m_model;
    m_model = nullptr;
    m_station = m_log->station();
    m_contest = m_log->isOpen() ? createContest(m_station.contestId) : nullptr;

    if (m_log->isOpen()) {
        m_model = new QSqlTableModel(this, m_log->database());
        m_model->setTable(QStringLiteral("qso"));
        m_model->setEditStrategy(QSqlTableModel::OnFieldChange);
        m_model->setSort(1, Qt::AscendingOrder);
        m_model->select();
        const QStringList headers{tr("Nr"), tr("Zeit (UTC)"), tr("kHz"), tr("Band"), tr("Mode"), tr("Call"),
                                  tr("RST s"), tr("Exch s"), tr("RST r"), tr("Exch r")};
        for (int i = 0; i < headers.size(); ++i)
            m_model->setHeaderData(i, Qt::Horizontal, headers.at(i));
        connect(m_model, &QSqlTableModel::dataChanged, this, &ContestPanel::updateScore);
        m_view->setModel(m_model);
        m_view->hideColumn(0);
        m_view->resizeColumnsToContents();
    } else {
        m_view->setModel(nullptr);
    }

    const bool usable = m_contest != nullptr;
    for (QLineEdit *field : {m_call, m_rst, m_exch})
        field->setEnabled(usable);
    if (m_contest)
        m_exch->setPlaceholderText(m_contest->exchangeLabel());
    wipe();
    updateScore();
    updateModeLabel();
}

MessageSet ContestPanel::messages(bool run) const
{
    MessageSet result;
    if (!m_contest)
        return result;
    const MessageSet defaults = m_contest->defaultMessages(run);
    QSettings settings;
    for (const QString &key : MessageKeys)
        result.insert(key, settings.value(settingsKey(m_contest->id(), run, key), defaults.value(key)).toString());
    return result;
}

void ContestPanel::setMessages(bool run, const MessageSet &messages)
{
    if (!m_contest)
        return;
    QSettings settings;
    for (auto it = messages.cbegin(); it != messages.cend(); ++it)
        settings.setValue(settingsKey(m_contest->id(), run, it.key()), it.value());
}

bool ContestPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(watched, event);
    auto *ke = static_cast<QKeyEvent *>(event);

    if (watched == m_view) {
        if (ke->key() == Qt::Key_Delete) {
            deleteSelectedQso();
            return true;
        }
        if (ke->key() == Qt::Key_Escape) {
            focusField(m_call);
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }

    auto *field = qobject_cast<QLineEdit *>(watched);
    if (field && handleKey(field, ke->key(), ke->modifiers()))
        return true;
    return QWidget::eventFilter(watched, event);
}

bool ContestPanel::handleKey(QLineEdit *field, int key, Qt::KeyboardModifiers mods)
{
    if (!m_contest)
        return false;
    const bool shift = mods & Qt::ShiftModifier;
    const bool ctrl = mods & Qt::ControlModifier;
    const bool alt = mods & Qt::AltModifier;
    const bool keypad = mods & Qt::KeypadModifier;

    // Function keys F1..F8: send, Shift = edit
    if (key >= Qt::Key_F1 && key <= Qt::Key_F8 && !ctrl && !alt) {
        const QString name = QStringLiteral("F%1").arg(key - Qt::Key_F1 + 1);
        if (shift)
            editMessage(name);
        else
            sendMessage(name);
        if (name == QLatin1String("F1"))
            m_cqPending = m_run && m_call->text().isEmpty();
        return true;
    }

    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (keypad && key == Qt::Key_Enter && !m_esm) {
            logQso();
            return true;
        }
        onEnter();
        return true;
    case Qt::Key_Insert:
    case Qt::Key_Semicolon:
        if (shift && key == Qt::Key_Insert) {
            editMessage(QStringLiteral("INS"));
            return true;
        }
        sendMessage(QStringLiteral("INS"));
        if (!m_call->text().isEmpty())
            focusField(m_exch);
        return true;
    case Qt::Key_Plus:
    case Qt::Key_Apostrophe:
        if (ctrl && shift) { // Ctrl-Shift-+ : edit message
            editMessage(QStringLiteral("PLUS"));
            return true;
        }
        if (ctrl) { // Ctrl-+ : log silently
            logQso();
            return true;
        }
        {
            QString error;
            if (!m_call->text().isEmpty() && m_contest->validateExchange(m_exch->text(), &error)) {
                sendMessage(QStringLiteral("PLUS"));
                logQso();
            } else {
                Q_EMIT statusMessage(error.isEmpty() ? tr("Rufzeichen fehlt") : error);
            }
        }
        return true;
    case Qt::Key_Escape:
        m_cqPending = false;
        if (m_keyer->isSending())
            m_keyer->stop();
        return true;
    case Qt::Key_F11:
        wipe();
        return true;
    case Qt::Key_W:
        if (alt) {
            wipe();
            return true;
        }
        if (ctrl) {
            field->clear();
            updateInfo();
            return true;
        }
        return false;
    case Qt::Key_M:
        if (ctrl) {
            toggleEsm();
            return true;
        }
        return false;
    case Qt::Key_R:
        if (ctrl) {
            toggleCqRepeat();
            return true;
        }
        return false;
    case Qt::Key_Tab:
        if (ctrl) {
            toggleRun();
            return true;
        }
        return false;
    case Qt::Key_Space:
        focusField(field == m_call ? m_exch : m_call);
        return true;
    case Qt::Key_PageUp:
        m_keyer->changeSpeed(+2);
        return true;
    case Qt::Key_PageDown:
        m_keyer->changeSpeed(-2);
        return true;
    case Qt::Key_F9:
        if (alt) {
            m_keyer->changeSpeed(-2);
            return true;
        }
        return false;
    case Qt::Key_F10:
        if (alt) {
            m_keyer->changeSpeed(+2);
            return true;
        }
        return false;
    case Qt::Key_T:
    case Qt::Key_K:
        if (alt) {
            openFreeText();
            return true;
        }
        return false;
    case Qt::Key_V:
        if (alt) {
            bool ok = false;
            const int wpm = QInputDialog::getInt(this, tr("CW-Tempo"), tr("WpM:"), m_keyer->speed(), 10, 60, 1, &ok);
            if (ok)
                m_keyer->setSpeed(wpm);
            return true;
        }
        return false;
    default:
        return false;
    }
}

void ContestPanel::sendMessage(const QString &key)
{
    if (!m_contest)
        return;
    const QString text = messages(m_run).value(key);
    if (text.isEmpty())
        return;

    MacroContext ctx;
    ctx.myCall = m_station.myCall;
    ctx.call = m_call->text();
    ctx.lastCall = m_lastCall;
    ctx.rst = QStringLiteral("599");
    ctx.serial = m_contest->exchangeSent(m_station, nextSerial());
    ctx.exchange = ctx.serial;
    ctx.cqZone = m_station.cqZone;
    const QString expanded = expandMacro(text, ctx);

    if (!checkCwReady()) {
        Q_EMIT statusMessage(tr("Nicht gesendet: %1").arg(expanded));
        return;
    }
    if (key != QLatin1String("F1"))
        m_cqPending = false;
    m_keyer->send(expanded);
}

bool ContestPanel::checkCwReady()
{
    if (m_radio->state() != flex::FlexRadio::State::Connected) {
        Q_EMIT statusMessage(tr("Kein Radio verbunden"));
        return false;
    }
    const auto slice = m_radio->txSlice();
    if (!slice) {
        Q_EMIT statusMessage(tr("Kein TX-Slice"));
        return false;
    }
    if (!slice->mode.startsWith(QLatin1String("CW"))) {
        Q_EMIT statusMessage(tr("TX-Slice %1 steht auf %2, nicht CW").arg(slice->letter, slice->mode));
        return false;
    }
    return true;
}

void ContestPanel::openFreeText()
{
    m_cqPending = false;
    FreeTextDialog dialog(m_keyer, [this] { return checkCwReady(); }, this);
    dialog.exec();
    if (!dialog.sentText().isEmpty())
        Q_EMIT statusMessage(tr("CW-Freitext: %1").arg(dialog.sentText()));
    focusField(m_call);
}

void ContestPanel::editMessage(const QString &key)
{
    MessageSet set = messages(m_run);
    bool ok = false;
    const QString text = QInputDialog::getText(this, tr("Nachricht bearbeiten"),
                                               tr("%1 (%2):\nVariablen: $MYCALL $CALL $LASTCALL $RST $SERIAL $CQZONE")
                                                   .arg(key, m_run ? tr("Run") : tr("S&P")),
                                               QLineEdit::Normal, set.value(key), &ok);
    if (!ok)
        return;
    set.insert(key, text);
    setMessages(m_run, set);
}

void ContestPanel::onEnter()
{
    const bool hasCall = !m_call->text().isEmpty();
    QString error;
    const bool exchOk = m_contest->validateExchange(m_exch->text(), &error);

    if (!m_esm) {
        logQso();
        return;
    }
    if (!hasCall) {
        if (m_run) {
            sendMessage(QStringLiteral("F1"));
            m_cqPending = true;
        } else {
            sendMessage(QStringLiteral("F4"));
        }
        return;
    }
    if (!exchOk) {
        sendMessage(QStringLiteral("INS"));
        focusField(m_exch);
        return;
    }
    sendMessage(QStringLiteral("PLUS"));
    logQso();
}

bool ContestPanel::logQso()
{
    if (!m_contest)
        return false;
    QString error;
    if (m_call->text().size() < 3) {
        Q_EMIT statusMessage(tr("Rufzeichen fehlt"));
        focusField(m_call);
        return false;
    }
    if (!m_contest->validateExchange(m_exch->text(), &error)) {
        Q_EMIT statusMessage(error);
        focusField(m_exch);
        return false;
    }
    const double khz = currentFreqKhz();
    const QString band = bandForKhz(khz);
    if (band.isEmpty()) {
        Q_EMIT statusMessage(tr("Keine gültige Frequenz vom Radio – QSO nicht geloggt"));
        return false;
    }

    Qso qso;
    qso.time = QDateTime::currentDateTimeUtc();
    qso.freqKhz = khz;
    qso.band = band;
    qso.mode = m_station.categoryMode == QLatin1String("SSB") ? QStringLiteral("PH") : m_station.categoryMode == QLatin1String("RTTY") ? QStringLiteral("RY") : QStringLiteral("CW");
    qso.call = m_call->text().trimmed().toUpper();
    qso.rstSent = QStringLiteral("599");
    qso.exchSent = m_contest->exchangeSent(m_station, nextSerial());
    qso.rstRcvd = m_rst->text().isEmpty() ? QStringLiteral("599") : m_rst->text();
    QString rcvd = m_exch->text().trimmed().toUpper();
    if (m_contest->usesSerial())
        rcvd = QString::number(rcvd.toInt());
    qso.exchRcvd = rcvd;

    const QString dupe = m_contest->dupeReason(qso, m_qsos);
    if (!m_log->addQso(qso)) {
        Q_EMIT statusMessage(tr("Fehler beim Speichern des QSO!"));
        return false;
    }
    m_lastCall = qso.call;
    if (m_model) {
        m_model->select();
        m_view->scrollToBottom();
    }
    Q_EMIT statusMessage(tr("Geloggt: %1 %2 %3%4").arg(qso.call, qso.band, qso.exchRcvd,
                                                        dupe.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(dupe)));
    wipe();
    return true;
}

void ContestPanel::wipe()
{
    m_call->clear();
    m_exch->clear();
    m_rst->setText(QStringLiteral("599"));
    focusField(m_call);
    updateInfo();
}

void ContestPanel::deleteSelectedQso()
{
    if (!m_model)
        return;
    const QModelIndex idx = m_view->currentIndex();
    if (!idx.isValid())
        return;
    const qint64 id = m_model->data(m_model->index(idx.row(), 0)).toLongLong();
    const QString call = m_model->data(m_model->index(idx.row(), 5)).toString();
    const QString time = m_model->data(m_model->index(idx.row(), 1)).toString();
    if (QMessageBox::question(this, tr("QSO löschen"), tr("QSO mit %1 (%2) löschen?").arg(call, time)) != QMessageBox::Yes)
        return;
    m_log->deleteQso(id);
    m_model->select();
    Q_EMIT statusMessage(tr("QSO mit %1 gelöscht").arg(call));
    focusField(m_call);
}

void ContestPanel::toggleRun()
{
    m_run = !m_run;
    m_cqPending = false;
    updateModeLabel();
}

void ContestPanel::toggleEsm()
{
    m_esm = !m_esm;
    updateModeLabel();
}

void ContestPanel::toggleCqRepeat()
{
    m_cqRepeat = !m_cqRepeat;
    if (!m_cqRepeat)
        m_cqPending = false;
    updateModeLabel();
}

void ContestPanel::focusField(QLineEdit *field)
{
    field->setFocus();
    field->end(false);
}

int ContestPanel::nextSerial() const
{
    int maxSerial = 0;
    for (const Qso &q : m_qsos)
        maxSerial = qMax(maxSerial, q.exchSent.toInt());
    return maxSerial + 1;
}

double ContestPanel::currentFreqKhz() const
{
    const auto slice = m_radio->txSlice();
    return slice ? slice->frequencyMHz * 1000.0 : 0.0;
}

QString ContestPanel::currentBand() const
{
    return bandForKhz(currentFreqKhz());
}

void ContestPanel::updateInfo()
{
    if (!m_contest) {
        m_infoLabel->setText(tr("Kein Log geöffnet – Datei → Neues Log …"));
        m_sentLabel->clear();
        return;
    }
    m_sentLabel->setText(tr("Sende: 599 %1").arg(m_contest->exchangeSent(m_station, nextSerial())));

    QStringList parts;
    const QString call = m_call->text().trimmed();
    if (call.size() >= 3) {
        Qso candidate;
        candidate.call = call;
        candidate.time = QDateTime::currentDateTimeUtc();
        candidate.band = currentBand();
        const QString dupe = m_contest->dupeReason(candidate, m_qsos);
        if (!dupe.isEmpty())
            parts << QStringLiteral("<b style='color:#c00'>%1</b>").arg(dupe.toHtmlEscaped());
        QStringList before;
        for (const Qso &q : m_qsos) {
            if (q.call == call)
                before << QStringLiteral("%1 %2").arg(q.band, q.time.toString(QStringLiteral("HH:mm")));
        }
        if (!before.isEmpty())
            parts << tr("vorher: %1").arg(before.join(QStringLiteral(", ")));
        else
            parts << QStringLiteral("<b style='color:#080'>%1</b>").arg(tr("neues Rufzeichen"));
    }
    const QString timeStatus = m_contest->timeStatus(QDateTime::currentDateTimeUtc());
    if (!timeStatus.isEmpty())
        parts << timeStatus;
    m_infoLabel->setText(parts.join(QStringLiteral(" · ")));
}

void ContestPanel::updateScore()
{
    m_qsos = m_log->qsos();
    if (!m_contest) {
        updateInfo();
        return;
    }
    Q_EMIT scoreChanged(m_contest->score(m_qsos, m_station));
    updateInfo();
}

void ContestPanel::updateModeLabel()
{
    QStringList parts;
    parts << (m_run ? QStringLiteral("RUN") : QStringLiteral("S&P"));
    if (m_esm)
        parts << QStringLiteral("ESM");
    if (m_cqRepeat)
        parts << QStringLiteral("CQ-Rep");
    parts << QStringLiteral("%1 WpM").arg(m_keyer->speed());
    if (m_keyer->isSending())
        parts << QStringLiteral("<b style='color:#c00'>TX</b>");
    m_modeLabel->setText(parts.join(QStringLiteral(" · ")));
}

} // namespace qrl
