// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio/Sidetone.h"

#include "tx/Macros.h"

#include <QAudioFormat>
#include <QAudioSink>
#include <QMediaDevices>
#include <QtEndian>

#include <cmath>
#include <cstring>

namespace qrl {

// --- ToneSource ------------------------------------------------------------------

ToneSource::ToneSource(QObject *parent)
    : QIODevice(parent)
{
    open(QIODevice::ReadOnly);
}

void ToneSource::enqueue(const QByteArray &pcm)
{
    QMutexLocker lock(&m_mutex);
    m_queue.append(pcm);
}

void ToneSource::clear()
{
    QMutexLocker lock(&m_mutex);
    m_queue.clear();
}

qint64 ToneSource::readData(char *data, qint64 maxSize)
{
    QMutexLocker lock(&m_mutex);
    maxSize &= ~qint64(1); // whole 16-bit samples
    const qint64 n = qMin<qint64>(maxSize, m_queue.size());
    if (n > 0) {
        std::memcpy(data, m_queue.constData(), size_t(n));
        m_queue.remove(0, n);
    }
    if (n < maxSize)
        std::memset(data + n, 0, size_t(maxSize - n)); // silence keeps the sink running
    return maxSize;
}

// --- Sidetone ------------------------------------------------------------------------

Sidetone::Sidetone(QObject *parent)
    : QObject(parent)
    , m_source(new ToneSource(this))
{
}

Sidetone::~Sidetone()
{
    if (m_sink)
        m_sink->stop();
}

void Sidetone::ensureSink()
{
    if (m_sink)
        return;
    QAudioFormat format;
    format.setSampleRate(SampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    m_sink = new QAudioSink(QMediaDevices::defaultAudioOutput(), format, this);
    m_sink->setBufferSize(SampleRate / 20 * 2); // ~50 ms
    m_sink->start(m_source);
}

void Sidetone::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled)
        stop();
}

void Sidetone::setVolume(double volume)
{
    m_volume = qBound(0.0, volume, 1.0);
}

void Sidetone::play(const QString &text, int wpm)
{
    if (!m_enabled)
        return;
    ensureSink();
    m_source->enqueue(render(text + QLatin1Char(' '), wpm, m_pitch, m_volume));
}

void Sidetone::stop()
{
    m_source->clear();
}

QByteArray Sidetone::render(const QString &text, int wpm, int pitchHz, double volume)
{
    if (wpm <= 0)
        return {};
    const int dit = SampleRate * 1200 / wpm / 1000;   // samples per dit
    const int ramp = SampleRate * 5 / 1000;           // 5 ms rise/fall
    const double phaseStep = 2.0 * M_PI * pitchHz / SampleRate;
    const double amplitude = 32767.0 * volume;

    QByteArray out;
    out.reserve(morseUnits(text) * dit * 2 + 1024);
    double phase = 0.0;

    auto appendSilence = [&](int samples) { out.append(QByteArray(samples * 2, '\0')); };
    auto appendTone = [&](int samples) {
        for (int i = 0; i < samples; ++i) {
            double env = 1.0;
            if (i < ramp)
                env = 0.5 - 0.5 * std::cos(M_PI * i / ramp);
            else if (i >= samples - ramp)
                env = 0.5 - 0.5 * std::cos(M_PI * (samples - i) / ramp);
            const auto v = qint16(std::lround(amplitude * env * std::sin(phase)));
            phase += phaseStep;
            char bytes[2];
            qToLittleEndian(v, bytes);
            out.append(bytes, 2);
        }
    };

    bool first = true;
    for (const QChar ch : text.toUpper()) {
        if (ch == QLatin1Char(' ')) {
            appendSilence(4 * dit);
            continue;
        }
        const char *code = morsePattern(ch);
        if (!code)
            continue;
        if (!first)
            appendSilence(3 * dit);
        first = false;
        for (int i = 0; code[i]; ++i) {
            if (i > 0)
                appendSilence(dit);
            appendTone(code[i] == '.' ? dit : 3 * dit);
        }
    }
    return out;
}

} // namespace qrl
