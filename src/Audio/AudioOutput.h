#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <QObject>
#include <QIODevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QMediaDevices>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include <QByteArray>
#include <QDebug>
#include <opus.h>

class AudioOutput : public QObject {
    Q_OBJECT
public:
    AudioOutput();
    ~AudioOutput();

    Q_INVOKABLE void start();


Q_SIGNALS:
    void newPacket();

public Q_SLOTS:
    void play();
    void addData(const QByteArray &data);

private:
    bool        m_started = false;
    QAudioFormat format;
    OpusDecoder *decoder;
    QAudioSink *audioSink;
    QIODevice *audioDevice;
    QMutex mutex;        
    QQueue<QByteArray> dataQueue;
};

#endif // AUDIOOUTPUT_H
