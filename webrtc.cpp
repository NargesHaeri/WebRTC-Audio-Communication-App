#include "webrtc.h"
#include <QtEndian>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <stdexcept>

static_assert(true);

#pragma pack(push, 1)
struct RtpHeader {
    uint8_t first;
    uint8_t marker:1;
    uint8_t payloadType:7;
    uint16_t sequenceNumber;
    uint32_t timestamp;
    uint32_t ssrc;
};
#pragma pack(pop)

WebRTC::WebRTC(QObject *parent)
    : QObject{parent},
    m_audio("Audio")
{
    m_gatheringComplited = false;
    m_instanceCounter++;

    connect(this, &WebRTC::gatheringComplited, [this] (const QString &peerID) {
        m_localDescription = descriptionToJson(m_peerConnections[peerID]->localDescription().value());
        Q_EMIT localDescriptionGenerated(peerID, m_localDescription);

        if (m_isOfferer)
            Q_EMIT this->offerIsReady(peerID, m_localDescription);
        else
            Q_EMIT this->answerIsReady(peerID, m_localDescription);
    });
}

WebRTC::~WebRTC()
{
    m_instanceCounter--;
    m_peerConnections.clear();
    m_peerTracks.clear();
}

/**
 * ====================================================
 * ================= public methods ===================
 * ====================================================
 */

void WebRTC::init(const QString &id, bool isOfferer)
{
    // Initialize WebRTC using libdatachannel library
    m_localId = id;
    m_isOfferer = isOfferer;

    // Create an instance of rtc::Configuration to set up ICE configuration
    m_config = rtc::Configuration();

    // Add a STUN server to help peers find their public IP addresses
    m_config.iceServers.emplace_back("stun:stun.l.google.com:19302");

    // Create and configure a TURN server for relaying media
    /*rtc::IceServer turnServer("turn:your-turn-server.com:3478", "username", "password");
    m_config.iceServers.push_back(turnServer);*/

    // Set up the audio stream configuration
    m_audio.setBitrate(m_bitRate);
    m_audio.addSSRC(m_ssrc,"audio");
    m_audio.addOpusCodec(m_payloadType);
}

void WebRTC::addPeer(const QString &peerId)
{
    // Create and add a new peer connection
    auto newPeer = std::make_shared<rtc::PeerConnection>(m_config);
    m_peerConnections.insert(peerId ,newPeer);

    //add audio track
    addAudioTrack(peerId, "audio");

    // Set up a callback for when the local description is generated
    newPeer->onLocalDescription([this, peerId](const rtc::Description &description) {
        // The local description should be emitted using the appropriate signals based on the peer's role (offerer or answerer)
        //m_peerSdps[peerId] = description;
        if (!m_gatheringComplited) {
            m_gatheringComplited = true;
            Q_EMIT gatheringComplited(peerId);
        }
    });

    // Set up a callback for handling local ICE candidates
    newPeer->onLocalCandidate([this, peerId](rtc::Candidate candidate) {
        // Emit the local candidates using the localCandidateGenerated signal
        Q_EMIT localCandidateGenerated(peerId, 
                                     QString::fromStdString(candidate.candidate()),
                                     QString::fromStdString(candidate.mid()));
    });

    // Set up a callback for when the state of the peer connection changes
    newPeer->onStateChange([this, peerId](rtc::PeerConnection::State state) {
        // Handle different states like New, Connecting, Connected, Disconnected, etc.
        switch (state) {
            case rtc::PeerConnection::State::Connected:
                break;
            case rtc::PeerConnection::State::Disconnected:
            case rtc::PeerConnection::State::Failed:
            case rtc::PeerConnection::State::Closed:
                break;
            default:
                break;
        }
    });

    // Set up a callback for monitoring the gathering state
    newPeer->onGatheringStateChange([this, peerId](rtc::PeerConnection::GatheringState state) {
        // When the gathering is complete, emit the gatheringComplited signal
        if (state == rtc::PeerConnection::GatheringState::Complete) {
            m_gatheringComplited = true;
            Q_EMIT gatheringComplited(peerId);
        }
    });

    // Set up a callback for handling incoming tracks
    newPeer->onTrack([this, peerId](std::shared_ptr<rtc::Track> track) {
        // handle the incoming media stream
        m_peerTracks[peerId] = track;
        
        track->onMessage([this, peerId](rtc::message_variant data) {
            QByteArray packet = readVariant(data);
            Q_EMIT incommingPacket(peerId, packet, packet.size());
        });
    });
}

void WebRTC::generateOfferSDP(const QString &peerId)
{
    // Set the local description for the peer's connection
    if (auto it = m_peerConnections.find(peerId); it != m_peerConnections.end()) {
        it.value()->setLocalDescription(rtc::Description::Type::Offer);
    }
}

void WebRTC::generateAnswerSDP(const QString &peerId)
{
    // Generate an answer SDP for the peer
    if (auto it = m_peerConnections.find(peerId); it != m_peerConnections.end()) {
        it.value()->setLocalDescription(rtc::Description::Type::Answer);
    }
}

void WebRTC::addAudioTrack(const QString &peerId, const QString &trackName)
{
    // Add an audio track to the peer connection
    if (auto it = m_peerConnections.find(peerId); it != m_peerConnections.end()) {
        auto track = it.value()->addTrack(m_audio);
        m_peerTracks[peerId] = track;
        
        // Handle track events
        track->onMessage([this, peerId](rtc::message_variant data) {
            QByteArray packet = readVariant(data);
            Q_EMIT incommingPacket(peerId, packet, packet.size());
        });

        track->onFrame([this](rtc::binary frame, rtc::FrameInfo info) {
            // Handle incoming audio frames if needed
        });
    }
}

void WebRTC::sendTrack(const QString &peerId, const QByteArray &buffer)
{
    if (auto it = m_peerTracks.find(peerId); it != m_peerTracks.end()) {
        try {
            // Create the RTP header and initialize an RtpHeader struct
            RtpHeader header;
            header.first = 0x80; // RTP version 2
            header.marker = 0;
            header.payloadType = m_payloadType;
            header.sequenceNumber = qToBigEndian(m_sequenceNumber++);
            header.timestamp = qToBigEndian(getCurrentTimestamp());
            header.ssrc = qToBigEndian(static_cast<uint32_t>(m_ssrc));

            // Create the RTP packet by appending the RTP header and the payload buffer
            QByteArray packet;
            packet.append(reinterpret_cast<const char*>(&header), sizeof(RtpHeader));
            packet.append(buffer);

            // Send the packet, catch and handle any errors that occur during sending
            it.value()->send(packet.toStdString());
        } catch (const std::exception &e) {
            qWarning() << "Failed to send track:" << e.what();
        }
    }
}

/**
 * ====================================================
 * ================= public slots =====================
 * ====================================================
 */

void WebRTC::setRemoteDescription(const QString &peerID, const QString &sdp)
{
    // Set the remote SDP description for the peer
    if (auto it = m_peerConnections.find(peerID); it != m_peerConnections.end()) {
        try {
            rtc::Description description(sdp.toStdString());
            it.value()->setRemoteDescription(description);
        } catch (const std::exception &e) {
            qWarning() << "Failed to set remote description:" << e.what();
        }
    }
}

void WebRTC::setRemoteCandidate(const QString &peerID, const QString &candidate, const QString &sdpMid)
{
    // Add remote ICE candidates to the peer connection
    if (auto it = m_peerConnections.find(peerID); it != m_peerConnections.end()) {
        try {
            it.value()->addRemoteCandidate(rtc::Candidate(candidate.toStdString(), sdpMid.toStdString()));
        } catch (const std::exception &e) {
            qWarning() << "Failed to add remote candidate:" << e.what();
        }
    }
}

/*
 * ====================================================
 * ================= private methods ==================
 * ====================================================
 */

QByteArray WebRTC::readVariant(const rtc::message_variant &data)
{
    // Utility function to read the rtc::message_variant into a QByteArray
    if (std::holds_alternative<std::string>(data)) {
        const auto &str = std::get<std::string>(data);
        return QByteArray::fromStdString(str);
    } else if (std::holds_alternative<rtc::binary>(data)) {
        const auto &bin = std::get<rtc::binary>(data);
        return QByteArray(reinterpret_cast<const char*>(bin.data()), static_cast<int>(bin.size()));
    }
    return QByteArray();
}

QString WebRTC::descriptionToJson(const rtc::Description &description)
{
    // Utility function to convert rtc::Description to JSON format
    QJsonObject sdpObj;
    sdpObj["type"] = QString::fromStdString(description.typeString());
    sdpObj["sdp"] = QString::fromStdString(description);
    return QJsonDocument(sdpObj).toJson(QJsonDocument::Compact);
}

int WebRTC::bitRate() const
{
    // Retrieves the current bit rate
    return m_bitRate;
}

void WebRTC::setBitRate(int newBitRate)
{
    // Set a new bit rate and emit the bitRateChanged signal
    if (m_bitRate != newBitRate) {
        m_bitRate = newBitRate;
        Q_EMIT bitRateChanged();
    }
}

void WebRTC::resetBitRate()
{
    // Reset the bit rate to its default value
    setBitRate(48000);
}

void WebRTC::setPayloadType(int newPayloadType)
{
    // Sets a new payload type and emit the payloadTypeChanged signal
    if (m_payloadType != newPayloadType) {
        m_payloadType = newPayloadType;
        Q_EMIT payloadTypeChanged();
    }
}

void WebRTC::resetPayloadType()
{
    // Resets the payload type to its default value
    setPayloadType(111);
}

rtc::SSRC WebRTC::ssrc() const
{
    // Retrieve the current SSRC value
    return m_ssrc;
}

void WebRTC::setSsrc(rtc::SSRC newSsrc)
{
    // Set a new SSRC and emit the ssrcChanged signal
    if (m_ssrc != newSsrc) {
        m_ssrc = newSsrc;
        Q_EMIT ssrcChanged();
    }
}

void WebRTC::resetSsrc()
{
    // Reset the SSRC to its default value
    setSsrc(2);
}

int WebRTC::payloadType() const
{
    // Retrieve the current payload type
    return m_payloadType;
}

/**
 * ====================================================
 * ================= getters setters ==================
 * ====================================================
 */

bool WebRTC::isOfferer() const
{
    return m_isOfferer;
}

void WebRTC::setIsOfferer(bool newIsOfferer)
{
    if (m_isOfferer != newIsOfferer) {
        m_isOfferer = newIsOfferer;
        Q_EMIT isOffererChanged();
    }
}

void WebRTC::resetIsOfferer()
{
    setIsOfferer(false);
}
