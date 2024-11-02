#include "AudioOutput.h"

AudioOutput::AudioOutput() {

    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(1);    
    format.setSampleFormat(QAudioFormat::Int16);

    int error;
    decoder = opus_decoder_create(48000,1, &error);
    audioSink = new QAudioSink(QMediaDevices::defaultAudioOutput(), format, this);
    connect(this, &AudioOutput::newPacket, this, &AudioOutput::play, Qt::ConnectionType::QueuedConnection);

}

AudioOutput::~AudioOutput(){
    opus_decoder_destroy(decoder);
}

void AudioOutput::start()
{
    if (m_started) return;

    audioDevice = audioSink->start();
    audioSink->setVolume(100);
    if (!audioDevice->open(QIODevice::WriteOnly)) {
        qFatal("Could not open the audio device");
    }
    m_started = true;

}

void AudioOutput::addData(const QByteArray &data) {

    QMutexLocker locker(&mutex);
    dataQueue.enqueue(data);
    Q_EMIT newPacket();
}

void AudioOutput::play() {

    QMutexLocker locker(&mutex);

    if (!dataQueue.isEmpty()) {

        QByteArray packet = dataQueue.dequeue();
        std::vector<opus_int16> decodedOutput(960);
        int decodedBytes = opus_decode(decoder,
                                            reinterpret_cast<const unsigned char*>(packet.data()),
                                            packet.size(),
                                            decodedOutput.data(),
                                            960,
                                            0) * 2;

        const char* outputToWrite = reinterpret_cast<const char*>(decodedOutput.data());
        qint64 bytesWritten = audioDevice->write(outputToWrite, decodedBytes);

    }

    else {
        qDebug() << "Data queue is empty, nothing to play.";
    }
}
