/**
 * @file webserver.js
 * Gateway from TCP to WebSocket & Create a Web Server HTTP on port 80
 * Launch it with this command line : node webserver.js
 * @author Alexandre.Tortevois
 */

// https://github.com/websockets/ws/blob/master/doc/ws.md
// https://nodejs.org/dist/latest-v12.x/docs/api/net.html

const http = require('http');
const url = require('url');
const path = require('path');
const fs = require('fs');
const WebSocket = require('ws');
const Socket = require('net').Socket;

const config = {
    tcp_port: 1998,
    ws_port: 8888
};

const wss = new WebSocket.Server({port: config.ws_port});
const tcp_socket = new Socket();

function log(message) {
    // https://developer.mozilla.org/fr/docs/Web/JavaScript/Reference/Objets_globaux/Date/toLocaleString
    console.log("%s\t%s", new Date().toLocaleString(), message);
}

const server = http.createServer(function (request, response) {
    log(request.method + '\t' + request.url);

    // Get URL
    let pathname = __dirname + url.parse(request.url).pathname
    // log('pathname: ' + pathname);

    // Define MIME Type
    const MIME_Type = {
        '.ico': 'image/x-icon',
        '.html': 'text/html',
        '.js': 'text/javascript',
        '.json': 'application/json',
        '.css': 'text/css',
        '.png': 'image/png',
        '.jpg': 'image/jpeg',
        // '.wav': 'audio/wav',
        // '.mp3': 'audio/mpeg',
        // '.svg': 'image/svg+xml',
        // '.pdf': 'application/pdf',
        // '.doc': 'application/msword',
        // '.eot': 'appliaction/vnd.ms-fontobject',
        // '.ttf': 'application/font-sfnt'
    };

    fs.exists(pathname, function (exist) {
        // Display Error 404
        if (!exist) {
            response.writeHead(404, {'Content-Type': 'text/html'});
            return response.end("404 Not Found");
        }

        //  Is it a directory ? Try to get index.html
        if (fs.statSync(pathname).isDirectory()) {
            pathname += 'index.html';
        }

        // Read the file
        fs.readFile(pathname, 'utf-8', function (err, content) {
            // Display Error 500
            if (err) {
                response.writeHead(500, {'Content-Type': 'text/html'});
                return response.end("500 Internal Server Error");
            }

            // Get the extension of the file
            let ext = path.parse(pathname).ext;

            response.writeHead(200, {'Content-Type': MIME_Type[ext] || 'text/plain'});
            response.write(content);
            response.end();
        });
    });
});

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
}, (60 * 60 * 1000)); // 1h

tcp_socket.connect({host: "localhost", port: config.tcp_port}, () => {
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
    server.listen(80, '0.0.0.0');
    log('HTTP Server is running');
});

// net.Socket Event: 'data'
tcp_socket.on('data', (data) => {
    let msg = data.toString().replace(/\n|\r/g, '');
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