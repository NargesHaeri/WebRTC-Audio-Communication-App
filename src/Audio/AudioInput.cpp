#include "AudioInput.h"

AudioInput::AudioInput() {

    open(QIODevice::WriteOnly);
    int error;
    opusEncoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_AUDIO, &error);
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    audioSource = new QAudioSource(format, this);
    connect(audioSource, &QAudioSource::stateChanged, this, &AudioInput::handleStateChanged);
}

AudioInput::~AudioInput()
{
    opus_encoder_destroy(opusEncoder);
}

qint64 AudioInput::writeData(const char *data, qint64 len) {
    
    std::vector<unsigned char> opusData(960);
    int frameSize = len / 2;

    int encodedBytes = opus_encode(opusEncoder,
                                   reinterpret_cast<const opus_int16 *>(data),
                                   frameSize,
                                   opusData.data(),
                                   opusData.size());

 
    QByteArray encodedOpusData(reinterpret_cast<const char *>(opusData.data()), encodedBytes);
    Q_EMIT newAudioData(encodedOpusData);

    return len;
}

qint64 AudioInput::readData(char *data, qint64 len) {
    return -1;
}

void AudioInput::start()
{
    audioSource->start(this);
}


void AudioInput::handleStateChanged(QAudio::State newState) {
    if (newState == QAudio::IdleState) {
        audioSource->stop();
        qDebug() << "Recording stopped (Idle State).";
    } else if (newState == QAudio::StoppedState) {
        if (audioSource->error() != QAudio::NoError) {
            qWarning() << "Audio error occurred!";
        }
    }
}
