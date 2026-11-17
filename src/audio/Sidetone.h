// SPDX-License-Identifier: GPL-3.0-or-later
//
// Computer sidetone: plays the CW text being sent on the local sound card
// (like DXLog's "Computer sidetone"), because CWX sent by a non-GUI client is
// not monitored by the GUI client's local sidetone.
#pragma once

#include <QByteArray>
#include <QIODevice>
#include <QMutex>
#include <QObject>

class QAudioSink;

namespace qrl {

// Pull-mode source: queued tone samples, silence when idle.
class ToneSource : public QIODevice
{
    Q_OBJECT

public:
    explicit ToneSource(QObject *parent = nullptr);
    void enqueue(const QByteArray &pcm);
    void clear();
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return 4096 + QIODevice::bytesAvailable(); }

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    QMutex m_mutex;
    QByteArray m_queue;
};

class Sidetone : public QObject
{
    Q_OBJECT

public:
    static constexpr int SampleRate = 48000;

    explicit Sidetone(QObject *parent = nullptr);
    ~Sidetone() override;

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }
    void setPitch(int hz) { m_pitch = qBound(200, hz, 1500); }
    void setVolume(double volume); // 0..1

    void play(const QString &text, int wpm);
    void stop();

    // Mono 16-bit PCM at SampleRate (exposed for tests).
    static QByteArray render(const QString &text, int wpm, int pitchHz, double volume);

private:
    void ensureSink();

    QAudioSink *m_sink = nullptr;
    ToneSource *m_source;
    bool m_enabled = false;
    int m_pitch = 600;
    double m_volume = 0.3;
};

} // namespace qrl
