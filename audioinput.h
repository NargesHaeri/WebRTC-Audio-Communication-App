#ifndef AUDIOINPUT_H
#define AUDIOINPUT_H

#include <QObject>
#include <QIODevice>
#include <QAudioSource>
#include <QAudioFormat>
#include <QDebug>
#include <opus.h>

class AudioInput : public QIODevice {
    Q_OBJECT
public:
    AudioInput();
    ~AudioInput();



    qint64 writeData(const char *data, qint64 len) override;
    qint64 readData(char *data, qint64 len) override;

    Q_INVOKABLE void start();

Q_SIGNALS:
    void newAudioData(const QByteArray &data);

private:
    OpusEncoder *opusEncoder;
    QAudioSource *audioSource;
    void handleStateChanged(QAudio::State newState);
};

#endif 
