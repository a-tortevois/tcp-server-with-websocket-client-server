// https://github.com/websockets/ws/blob/master/doc/ws.md
// https://nodejs.org/dist/latest-v12.x/docs/api/net.html

const WebSocket = require('ws');
const wss = new WebSocket.Server({port: 8888});

const Socket = require('net').Socket;
const tcp_socket = new Socket();

function log(message) {
    // https://developer.mozilla.org/fr/docs/Web/JavaScript/Reference/Objets_globaux/Date/toLocaleString
    console.log("%s: %s", new Date().toLocaleString(), message);
}

function pingCallback() {
}

function heartbeat() {
    this.isAlive = true;
}

const interval = setInterval(() => {
    log('Send Ping');
    wss.clients.forEach(function each(ws) {
        if (ws.isAlive === false) return ws.terminate();
        ws.isAlive = false;
        ws.ping(pingCallback);
    });
}, 30000);

tcp_socket.connect({host: "localhost", port: 1998}, () => {
    log('TCP Socket: connection request');
});

// net.Socket Event: 'close'
tcp_socket.on('close', () => {
    log('TCP Socket: Close');
    // TODO try to reconnect
});

// net.Socket Event: 'connect'
tcp_socket.on('connect', () => {
    log('TCP Socket: connected');
});

// net.Socket Event: 'data'
tcp_socket.on('data', (data) => {
    let msg = data.toString().replace(/\n|\r/g,'');
    log('TCP Socket: data received from the server: ' + msg);

    // Broadcast to all connected clients
    wss.clients.forEach(function each(client) {
        if (client.readyState === WebSocket.OPEN) {
            client.send(msg);
        }
    });
});

// net.Socket Event: 'drain'

// net.Socket Event: 'end'
tcp_socket.on('end', () => {
    log('TCP Socket: end request');
});

// net.Socket Event: 'error'
tcp_socket.on('error', (error) => {
    log('TCP Socket: ' + error);
    // TODO check error, try to reconnect
});

// net.Socket Event: 'lookup'
// net.Socket Event: 'ready'
// net.Socket Event: 'timeout'

// WebSocket.Server Event: 'close'
wss.on('close', () => {
    log('WebSocket Server: Close');
    clearInterval(interval);
});

// WebSocket.Server Event: 'connection'
wss.on('connection', function connection(ws, req) {
    ws.ip = req.connection.remoteAddress + ':' + req.connection.remotePort;
    ws.isAlive = true;

    log('WebSocket.Server: New connexion from ' + ws.ip);

    //  WebSocket Event: 'close'
    ws.on('close', () => {
        log('WebSocket: Close ' + ws.ip);
    });

    //  WebSocket Event: 'error'
    ws.on('error', (error) => {
        log('WebSocket: Error on ' + ws.ip + ': ' + error);
    });

    //  WebSocket Event: 'message'
    ws.on('message', function incoming(message) {
        log('WebSocket message: ' + message);
        // '{"query":"GET","param":{"i":[]}}'
        tcp_socket.write(message);
    });

    // WebSocket Event: 'open'
    // WebSocket Event: 'ping'

    // WebSocket Event: 'pong'
    ws.on('pong', () => {
        log('WebSocket: Pong from ' + ws.ip);
        heartbeat();
    });

    // WebSocket Event: 'unexpected-response'
    // WebSocket Event: 'upgrade'
});

// WebSocket.Server Event: 'error'
wss.on('error', (error) => {
    log('WebSocket.Server Error: ' + error);
});

// WebSocket.Server Event: 'headers'
// WebSocket.Server Event: 'listening'