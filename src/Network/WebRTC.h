#ifndef WEBRTC_H
#define WEBRTC_H

#include <QObject>
#include <QMap>
#include <rtc/rtc.hpp>

class WebRTC : public QObject
{
    Q_OBJECT

public:
    explicit WebRTC(QObject *parent = nullptr);
    virtual ~WebRTC();

    Q_INVOKABLE void init(const QString &id, bool isOfferer = false);
    Q_INVOKABLE void addPeer(const QString &peerId);
    Q_INVOKABLE void generateOfferSDP(const QString &peerId);
    Q_INVOKABLE void generateAnswerSDP(const QString &peerId);
    Q_INVOKABLE void addAudioTrack(const QString &peerId, const QString &trackName);
    Q_INVOKABLE void sendTrack(const QString &peerId, const QByteArray &buffer);

    bool isOfferer() const;
    void setIsOfferer(bool newIsOfferer);
    void resetIsOfferer();

    rtc::SSRC ssrc() const;
    void setSsrc(rtc::SSRC newSsrc);
    void resetSsrc();

    int payloadType() const;
    void setPayloadType(int newPayloadType);
    void resetPayloadType();

    int bitRate() const;
    void setBitRate(int newBitRate);
    void resetBitRate();

Q_SIGNALS:

    void incommingPacket(const QString &peerId, const QByteArray &data, qint64 len);

    void localDescriptionGenerated(const QString &peerID, const QString &sdp);

    void localCandidateGenerated(const QString &peerID, const QString &candidate, const QString &sdpMid);

    void isOffererChanged();

    void connectionReady();

    void gatheringComplited(const QString &peerID);

    void offerIsReady(const QString &peerID, const QString& description);

    void answerIsReady(const QString &peerID, const QString& description);

    void ssrcChanged();

    void payloadTypeChanged();

    void bitRateChanged();



public Q_SLOTS:
    void setRemoteDescription(const QString &peerID, const QString &sdp);
    void setRemoteCandidate(const QString &peerID, const QString &candidate, const QString &sdpMid);

private:
    QByteArray readVariant(const rtc::message_variant &data);
    QString descriptionToJson(const rtc::Description &description);

    inline uint32_t getCurrentTimestamp() {
        using namespace std::chrono;
        auto now = steady_clock::now();
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count();
        return static_cast<uint32_t>(ms);
    }

private:
    static inline uint16_t                              m_sequenceNumber = 0;
    static inline uint32_t                              m_instanceCounter = 0;
    bool                                                m_gatheringComplited = false;
    int                                                 m_bitRate = 48000;
    int                                                 m_payloadType = 111;
    rtc::Description::Audio                             m_audio;
    rtc::SSRC                                           m_ssrc = 2;
    bool                                                m_isOfferer = false;
    QString                                             m_localId;
    rtc::Configuration                                  m_config;
    QMap<QString, rtc::Description>                     m_peerSdps;
    QMap<QString, std::shared_ptr<rtc::PeerConnection>> m_peerConnections;
    QMap<QString, std::shared_ptr<rtc::Track>>          m_peerTracks;
    QString                                             m_localDescription;
    QString                                             m_remoteDescription;


    Q_PROPERTY(bool isOfferer READ isOfferer WRITE setIsOfferer RESET resetIsOfferer NOTIFY isOffererChanged FINAL)
    Q_PROPERTY(rtc::SSRC ssrc READ ssrc WRITE setSsrc RESET resetSsrc NOTIFY ssrcChanged FINAL)
    Q_PROPERTY(int payloadType READ payloadType WRITE setPayloadType RESET resetPayloadType NOTIFY payloadTypeChanged FINAL)
    Q_PROPERTY(int bitRate READ bitRate WRITE setBitRate RESET resetBitRate NOTIFY bitRateChanged FINAL)
};

#endif
