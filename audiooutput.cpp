#include "audiooutput.h"

AudioOutput::AudioOutput() {

    QAudioFormat format;
    format.setSampleRate(48000);    // 48kHz sampling rate
    format.setChannelCount(1);      // Mono audio
    format.setSampleFormat(QAudioFormat::Int16);


    int error;
    decoder = opus_decoder_create(48000,1, &error);

    // Create QAudioSink
    audioSink = new QAudioSink(QMediaDevices::defaultAudioOutput(), format, this);

    // Connect the newPacket signal to the play slot
    connect(this, &AudioOutput::newPacket, this, &AudioOutput::play, Qt::ConnectionType::QueuedConnection);

    // Start QIODevice with QAudioSink
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
        qFatal("Shit");
    }

    m_started = true;

}

void AudioOutput::addData(const QByteArray &data) {
    // Lock the mutex to protect the data queue
    QMutexLocker locker(&mutex);

    // Add the new data to the queue
    dataQueue.enqueue(data);

    // Q_EMIT signal to notify that new data is available
    Q_EMIT newPacket();
}

void AudioOutput::play() {
    // Lock the mutex to protect the data queue
    qDebug() << "ah shit here we go again 2";
    QMutexLocker locker(&mutex);

    if (!dataQueue.isEmpty()) {
        // Get the first packet from the queue
        qDebug() << "ah shit here we go again 3";
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

        // For now, just write the raw audio data to the device without decoding
        qDebug() << "ah shit here we go again 4";

        qDebug() << "ah shit here we go again 5";
        if (bytesWritten != packet.size()) {
            qWarning() << "Not all audio data was written!";
        }

        // TODO: Add Opus decoding here before writing to the audio device
    } else {
        qDebug() << "Data queue is empty, nothing to play.";
    }
}
