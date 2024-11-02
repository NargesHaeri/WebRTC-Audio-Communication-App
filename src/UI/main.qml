import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import Audio
import WebRTCModule
import QtWebSockets

Window {

    width: 280
    height: 520
    visible: true
    title: qsTr("CA1")

    AudioOutput {
        id: output

        Component.onCompleted: {
            output.start();
        }

        function addIncomingData(data) {
            addData(data);
        }
    }

    AudioInput {
        id: input
        onNewAudioData: (barray) => {
                            if (textfield.text !== "") {
                                webrtc.sendTrack(textfield.text, barray);
                            }
                        }
    }

    WebRTC {
        id: webrtc

        Component.onCompleted: {
            webrtc.init(textfield.text, true);
        }

        onOfferIsReady: (peerID, sdp) => {
                            signalingServer.sendOffer(peerID, sdp);
                        }
        onAnswerIsReady: (peerID, sdp) => {
                             signalingServer.sendAnswer(peerID, sdp);
                         }

        onIncommingPacket: (peerId, packet, size) => {
                               output.addIncomingData(packet);
                           }
                           
        onConnectionReady: {
            input.start();
        }
    }

    WebSocket {
        id: signalingServer
        url: "ws://127.0.0.1:3000"

        onStatusChanged: {
                console.log("WebSocket status:", signalingServer.status);
                if (signalingServer.status === WebSocket.Open) {
                    console.log("WebSocket is open.");
                } else {
                    console.log("WebSocket is not open.");
                }
            }

        onTextMessageReceived: (message) => {

            const pmessage = JSON.parse(message);

            if (pmessage.type === "offer") {
                webrtc.addPeer(pmessage.fromId);
                webrtc.setRemoteDescription(pmessage.fromId, pmessage.sdp);
            } else if (pmessage.type === "answer") {
                webrtc.setRemoteDescription(pmessage.fromId, pmessage.sdp);
            } else if (pmessage.type === "ice_candidate") {
                webrtc.setRemoteCandidate(pmessage.fromId, pmessage.candidate, pmessage.mid);
            }
        }

        function sendOffer(targetId, sdp) {
            const message = {
                type: "offer",
                targetId: targetId,
                sdp: sdp
            };
            sendTextMessage(JSON.stringify(message));
        }

        function sendAnswer(targetId, sdp) {
            const message = {
                type: "answer",
                targetId: targetId,
                sdp: sdp
            };
            sendTextMessage(JSON.stringify(message));
        }

        function sendIceCandidate(targetId, candidate, mid) {
            const message = {
                type: "ice_candidate",
                targetId: targetId,
                candidate: candidate,
                mid: mid
            };
            sendTextMessage(JSON.stringify(message));
        }
        active: true;
    }

    Item {
        anchors.fill: parent

        ColumnLayout {
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                bottom: textfield.top
                margins: 20
            }

            Label {
                text: "IP: " + "172.16.142.176"
                Layout.fillWidth: true
                Layout.preferredHeight: 40
            }
            Label {
                text: "Ice Candidate: " + "172.16.142.176"
                Layout.fillWidth: true
                Layout.preferredHeight: 40
            }
            Label {
                id: callerid
                text: "Caller ID: " + textfield.text
                Layout.fillWidth: true
                Layout.preferredHeight: 40
            }
        }

        TextField {
            id: textfield
            placeholderText: "Peer ID"
            anchors.bottom: callbtn.top
            anchors.bottomMargin: 10
            anchors.left: callbtn.left
            anchors.right: callbtn.right
            enabled: !callbtn.pushed
        }

        Button {
            id: callbtn

            property bool pushed: false

            height: 47
            text: "Call"
            Material.background: "green"
            Material.foreground: "white"
            anchors {
                bottom: parent.bottom
                left: parent.left
                right: parent.right
                margins: 20
            }

            onClicked: {
                pushed = !pushed;
                if (pushed) {
                    Material.background = "red";
                    text = "End Call";
                    webrtc.addPeer(textfield.text);

                    if (webrtc.isOfferer) {
                        webrtc.generateOfferSDP(textfield.text);
                    }
                } else {
                    Material.background = "green";
                    text = "Call";
                }
            }

        }
    }
}
