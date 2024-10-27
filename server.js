const WebSocket = require('ws');
const wss = new WebSocket.Server({ port: 3000 });

// Hardcoded caller IDs and their connections
let users = {};
let usersCount = -1;
const id = "client";

wss.on('connection', (socket) => {
    usersCount++;
    const clientId = `${id}${usersCount}`;
    users[clientId] = socket;

    console.log('New connection:', clientId);

    socket.on('message', (message) => {
        const data = JSON.parse(message);

        // Handle offer
        if (data.type === 'offer') {
            const targetSocket = users[data.targetId];
            if (targetSocket) {
                targetSocket.send(JSON.stringify({
                    type: 'offer',
                    sdp: data.sdp,
                    fromId: clientId
                }));
            }
        }

        // Handle answer
        else if (data.type === 'answer') {
            const targetSocket = users[data.targetId];
            if (targetSocket) {
                targetSocket.send(JSON.stringify({
                    type: 'answer',
                    sdp: data.sdp,
                    fromId: clientId
                }));
            }
        }

        // Handle ICE candidate
        else if (data.type === 'ice_candidate') {
            const targetSocket = users[data.targetId];
            if (targetSocket) {
                targetSocket.send(JSON.stringify({
                    type: 'ice_candidate',
                    candidate: data.candidate,
                    mid: data.mid,
                    fromId: clientId
                }));
            }
        }
    });

    // Handle disconnection
    socket.on('close', () => {
        delete users[clientId];
        console.log(`Client disconnected: ${clientId}`);
    });
});
