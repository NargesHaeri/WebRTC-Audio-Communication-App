const WebSocket = require('ws');
const wss = new WebSocket.Server({ port: 3000 });

let users = {};
let usersCount = -1;
const id = "client";

console.log('Websocket is listening on port 3000');

wss.on('connection', (socket) => {
    usersCount++;
    const clientId = `${id}${usersCount}`;
    users[clientId] = socket;
    console.log('New connection:', clientId);

    socket.on('message', (message) => {
        const data = JSON.parse(message);

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

    socket.on('close', () => {
        delete users[clientId];
        console.log(`Client disconnected: ${clientId}`);
    });
});
