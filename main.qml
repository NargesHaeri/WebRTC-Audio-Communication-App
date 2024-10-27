import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import Audio
import WebRTCModule
import QtWebSockets  // Import WebSocket module

Window {
    x: 0
    y: 0
    width: 280
    height: 520
    visible: true
    title: qsTr("CA1")

    AudioOutput {
        id: output

        Component.onCompleted: {
            output.start();
            input.start();
        }
    }

    AudioInput {
        id: input
        onNewAudioData: (barray) => output.addData(barray)
    }

    WebRTC {
        id: webrtc

        Component.onCompleted: {
            webrtc.init(textfield.text, true);  // Initialize WebRTC
        }

        onOfferIsReady: (peerID, sdp) => {
            signalingServer.sendOffer(peerID, sdp)  // Send offer via WebSocket
        }
        onAnswerIsReady: (peerID, sdp) => {
            signalingServer.sendAnswer(peerID, sdp)  // Send offer via WebSocket
        }
    }

    WebSocket {
        id: signalingServer
        url: "ws://localhost:3000"  // URL of the server.js signaling server
        onTextMessageReceived: {
            // Parse incoming messages
            const message = JSON.parse(message);

            if (message.type === "offer") {
                // Handle 'offer' message from server
                webrtc.setRemoteDescription(message.fromId, message.sdp);
            } else if (message.type === "answer") {
                // Handle 'answer' message from server
                webrtc.setRemoteDescription(message.fromId, message.sdp);
            } else if (message.type === "ice_candidate") {
                // Handle ICE candidates if needed
                webrtc.setRemoteCandidate(message.fromId, message.candidate, message.mid);
            }
        }

        // Function to send an offer over WebSocket
        function sendOffer(targetId, sdp) {
            const message = {
                type: "offer",
                targetId: targetId,
                sdp: sdp
            };
            sendTextMessage(JSON.stringify(message));
        }

        // Function to send an answer over WebSocket
        function sendAnswer(targetId, sdp) {
            const message = {
                type: "answer",
                targetId: targetId,
                sdp: sdp
            };
            sendTextMessage(JSON.stringify(message));
        }

        // Function to send ICE candidates over WebSocket
        function sendIceCandidate(targetId, candidate, mid) {
            const message = {
                type: "ice_candidate",
                targetId: targetId,
                candidate: candidate,
                mid: mid
            };
            sendTextMessage(JSON.stringify(message));
        }
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
                text: "Ip: " + "172.16.142.176"
                Layout.fillWidth: true
                Layout.preferredHeight: 40
            }
            Label {
                text: "IceCandidate: " + "172.16.142.176"
                Layout.fillWidth: true
                Layout.preferredHeight: 40
            }
            Label {
                text: "CallerId: " + textfield.text
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
                pushed = !pushed
                if (pushed) {
                    Material.background = "red"
                    text = "End Call"
                    webrtc.addPeer(textfield.text)
                } else {
                    Material.background = "green"
                    text = "Call"
                    textfield.clear()
                }
            }
        }
    }
}
