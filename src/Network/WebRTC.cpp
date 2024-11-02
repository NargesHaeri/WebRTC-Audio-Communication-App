#include "WebRTC.h"
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


void WebRTC::init(const QString &id, bool isOfferer)
{
    // Initialize WebRTC using libdatachannel library
    m_localId = id;
    m_isOfferer = isOfferer;

    // Create an instance of rtc::Configuration to set up ICE configuration
    m_config = rtc::Configuration();

    // Add a STUN server to help peers find their public IP addresses
    m_config.iceServers.emplace_back("stun:stun.l.google.com:19302");

    // Set up the audio stream configuration
    m_audio.setBitrate(m_bitRate);
    m_audio.addSSRC(m_ssrc,"audio");
    m_audio.addOpusCodec(m_payloadType);
}


// Set the local description for the peer's connection
void WebRTC::generateOfferSDP(const QString &peerId)
{
    if (auto it = m_peerConnections.find(peerId); it != m_peerConnections.end()) {
        try {
            it.value()->setLocalDescription(rtc::Description::Type::Offer);
        } catch (const std::exception &e) {
            qWarning() << "Failed to set Offer SDP for peer:" << peerId << e.what();
        }
    } else {
        qWarning() << "Peer ID not found in connection map for Offer SDP generation:" << peerId;
    }
}


// Generate an answer SDP for the peer
void WebRTC::generateAnswerSDP(const QString &peerId)
{
    if (auto it = m_peerConnections.find(peerId); it != m_peerConnections.end()) {
        try {
            it.value()->setLocalDescription(rtc::Description::Type::Answer);
        } catch (const std::exception &e) {
            qWarning() << "Failed to set Answer SDP for peer:" << peerId << e.what();
        }
    } else {
        qWarning() << "Peer ID not found in connection map for Answer SDP generation:" << peerId;
    }
}


void WebRTC::addPeer(const QString &peerId)
{
 
    // Create and add a new peer connection
    auto peerConnection = std::make_shared<rtc::PeerConnection>(m_config);
    m_peerConnections[peerId] = peerConnection;

    // Set up a callback for when the local description is generated
    peerConnection->onLocalDescription([this, peerId](const rtc::Description &description) {
        if (!m_gatheringComplited) return;
    });

    // Set up a callback for handling local ICE candidates
    peerConnection->onLocalCandidate([this, peerId](rtc::Candidate candidate) {
        if (!m_gatheringComplited) return;
        Q_EMIT localCandidateGenerated(peerId,
                                       QString::fromStdString(candidate.candidate()),
                                       QString::fromStdString(candidate.mid()));
    });


    // Set up a callback for when the state of the peer connection changes
    peerConnection->onStateChange([this, peerId](rtc::PeerConnection::State state) {
        switch (state) {
        case rtc::PeerConnection::State::Connected:
            qDebug() << "Peer" << peerId << "connected";
            break;
        case rtc::PeerConnection::State::Disconnected:
            qDebug() << "Peer" << peerId << "disconnected";
            break;
        case rtc::PeerConnection::State::Failed:
            qDebug() << "Peer" << peerId << "connection failed";
            break;
        case rtc::PeerConnection::State::Closed:
            qDebug() << "Peer" << peerId << "connection closed";
            break;
        case rtc::PeerConnection::State::Connecting:
            qDebug() << "Peer" << peerId << "connecting...";
            break;
        default:
            qDebug() << "Peer" << peerId << "state changed to" << static_cast<int>(state);
            break;
        }
    });


    // Set up a callback for monitoring the gathering state
    peerConnection->onGatheringStateChange([this, peerId](rtc::PeerConnection::GatheringState state) {
        if (state == rtc::PeerConnection::GatheringState::Complete) {
            m_gatheringComplited = true;
            Q_EMIT gatheringComplited(peerId);
        }
    });

    // Set up a callback for handling incoming tracks
    peerConnection->onTrack([this, peerId](std::shared_ptr<rtc::Track> track) {
        if (track) {
            m_peerTracks[peerId] = track;
            
            track->onMessage([this, peerId](rtc::message_variant data) {
                QByteArray packet = readVariant(data);
                Q_EMIT incommingPacket(peerId, packet, packet.size());
            });

        } else {
            qWarning() << "Failed to create track for peer" << peerId;
        }
    });

    addAudioTrack(peerId, "audio");
}


void WebRTC::addAudioTrack(const QString &peerId, const QString &trackName)
{
    // Verify if the peer connection for this peerId exists
    if (auto it = m_peerConnections.find(peerId); it != m_peerConnections.end()) {
        auto peerConnection = it.value();

        // Add an audio track to the peer connection
        auto track = peerConnection->addTrack(m_audio);

        // Check if the track was successfully created
        if (!track) {
            qWarning() << "Failed to create audio track for peer" << peerId;
            return;
        }

        // Store the track in the peer tracks map
        m_peerTracks[peerId] = track;

        // Set up event handlers for the track
        track->onOpen([this, peerId]() {
            qDebug() << "Track for peer" << peerId << "is now open";
            Q_EMIT connectionReady();
        });

        track->onClosed([ peerId]() {
            //qDebug() << "Track for peer" << peerId << "is closed";
        });

        // Handle incoming messages on the track
        track->onMessage([this, peerId](rtc::message_variant data) {
            QByteArray packet = readVariant(data);
            Q_EMIT incommingPacket(peerId, packet, packet.size());
        });


    } else {
        qWarning() << "Peer connection for peerId" << peerId << "not found when adding audio track";
    }
}


void WebRTC::sendTrack(const QString &peerId, const QByteArray &buffer)
{
    if (auto it = m_peerTracks.find(peerId); it != m_peerTracks.end()) {
        auto track = it.value();
        if (!track->isOpen()) {
            return;  
        }

        try {
            // Create the RTP header and initialize an RtpHeader struct
            RtpHeader header;
            header.first = 0x80;
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
            track->send(packet.toStdString());
        } catch (const std::exception &e) {
            qWarning() << "Failed to send track:" << e.what();
        }
    } else {
        qWarning() << "Track for peer" << peerId << "not found in m_peerTracks.";
    }
}


/**
 * ====================================================
 * ================= public slots =====================
 * ====================================================
 */


// Set the remote SDP description for the peer that contains metadata about the media being transmitted
void WebRTC::setRemoteDescription(const QString &peerId, const QString &sdp)
{
    if (!m_peerConnections.contains(peerId))
        addPeer(peerId);
    std::shared_ptr<rtc::PeerConnection> connection = m_peerConnections[peerId];
    QJsonDocument doc = QJsonDocument::fromJson(sdp.toUtf8());
    QJsonObject jsonObj = doc.object();
    QString type = jsonObj.value("type").toString();
    QString sdpValue = jsonObj.value("sdp").toString();
    m_isOfferer = (type != "offer");
    connection->setRemoteDescription(rtc::Description(sdpValue.toStdString(), type.toStdString()));
}

// Add remote ICE candidates to the peer connection
void WebRTC::setRemoteCandidate(const QString &peerID, const QString &candidate, const QString &sdpMid)
{
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


// Utility function to read the rtc::message_variant into a QByteArray
QByteArray WebRTC::readVariant(const rtc::message_variant &data)
{
    QByteArray resultData;
    if (std::holds_alternative<rtc::binary>(data)) {
        const rtc::binary &binData = std::get<rtc::binary>(data);
        resultData =  QByteArray(reinterpret_cast<const char *>(binData.data()),
                          static_cast<int>(binData.size()));

    } else if (std::holds_alternative<std::string>(data)) {
        const std::string &strData = std::get<std::string>(data);
        resultData =  QByteArray(strData.c_str(), static_cast<int>(strData.size()));
    }
    if (resultData.size())
        resultData.remove(0, sizeof(RtpHeader));

    return resultData;
}

// Utility function to convert rtc::Description to JSON format
QString WebRTC::descriptionToJson(const rtc::Description &description)
{
    QJsonObject sdpObj;
    sdpObj["type"] = QString::fromStdString(description.typeString());
    sdpObj["sdp"] = QString::fromStdString(description);
    return QJsonDocument(sdpObj).toJson(QJsonDocument::Compact);
}

int WebRTC::bitRate() const
{
    return m_bitRate;
}

void WebRTC::setBitRate(int newBitRate)
{
    if (m_bitRate != newBitRate) {
        m_bitRate = newBitRate;
        Q_EMIT bitRateChanged();
    }
}

void WebRTC::resetBitRate()
{
    setBitRate(48000);
}

void WebRTC::setPayloadType(int newPayloadType)
{
    if (m_payloadType != newPayloadType) {
        m_payloadType = newPayloadType;
        Q_EMIT payloadTypeChanged();
    }
}

void WebRTC::resetPayloadType()
{
    setPayloadType(111);
}

rtc::SSRC WebRTC::ssrc() const
{
    return m_ssrc;
}

void WebRTC::setSsrc(rtc::SSRC newSsrc)
{
    if (m_ssrc != newSsrc) {
        m_ssrc = newSsrc;
        Q_EMIT ssrcChanged();
    }
}

void WebRTC::resetSsrc()
{
    setSsrc(2);
}

int WebRTC::payloadType() const
{
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
