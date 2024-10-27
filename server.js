// server.js
const io = require('socket.io')(3000);

// Hardcoded caller IDs and their connections
let usersCount = -1;
const id = "client"

let users = {};

io.on('connection', (socket) => {
    usersCount++;
    console.log('New connection:', socket.id)
    users[`${id}${usersCount}`] = socket;

  
    socket.on('offer', (data) => {
        const user = users[data.targetId]
        if (!user) return; 

        user.emit('offer', {
            sdp: data.sdp,
            fromId: socket.id
        });
    });

    // Handle SDP answer
    socket.on('answer', (data) => {
        const user = users[data.targetId]
        if (!user) return; 

        user.emit('answer', {
            sdp: data.sdp,
            fromId: socket.id
        });
    });

    // Handle ICE candidates
    socket.on('ice_candidate', (data) => {
        const targetSocketId = clients.get(data.targetId);
        if (targetSocketId) {
            io.to(targetSocketId).emit('ice_candidate', {
                candidate: data.candidate,
                mid: data.mid,
                fromId: socket.clientId
            });
        }
    });

    // Handle disconnection
    socket.on('disconnect', () => {
        if (socket.clientId) {
            clients.delete(socket.clientId);
            console.log(`Client disconnected: ${socket.clientId}`);
        }
    });
});

