const http = require('http');
const fs = require('fs');
const path = require('path');
const url = require('url');
const net = require('net');
const WebSocket = require('ws');
const { Aedes } = require('aedes');

const PORT = process.env.PORT || 3000;
const MQTT_PORT = 1883;
const PUBLIC_DIR = __dirname;

const aedes = new Aedes();

const MIME_TYPES = {
    '.html': 'text/html; charset=utf-8',
    '.js': 'text/javascript; charset=utf-8',
    '.css': 'text/css; charset=utf-8',
    '.json': 'application/json',
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
    '.jpeg': 'image/jpeg',
    '.gif': 'image/gif',
    '.svg': 'image/svg+xml',
    '.ico': 'image/x-icon',
};

// 1. Create Local Embedded MQTT Server on Port 1883
const mqttServer = net.createServer(aedes.handle);

mqttServer.listen(MQTT_PORT, () => {
    console.log(`=======================================================`);
    console.log(` 🚀 LOCAL MQTT BROKER ACTIVE ON PORT ${MQTT_PORT}`);
    console.log(` ESP32 Local MQTT Host: 192.168.181.98:${MQTT_PORT}`);
    console.log(`=======================================================`);
});

// Intercept published MQTT messages from ESP32 & broadcast to Dashboard
aedes.on('publish', (packet, client) => {
    if (packet && packet.topic === 'cit/lift/telemetry') {
        const payloadStr = packet.payload.toString();
        console.log(`📡 Local MQTT Telemetry Received from ESP32:`, payloadStr);
        broadcastTelemetry(payloadStr);
    }
});

aedes.on('client', (client) => {
    if (client) {
        console.log(`🔌 Hardware device connected to Local MQTT Broker: ${client.id}`);
    }
});

let resetPending = false;

// 2. Create HTTP Web Server
const server = http.createServer((req, res) => {
    let parsedUrl = url.parse(req.url, true);
    let parsedPath = parsedUrl.pathname;
    
    // API endpoint for Direct HTTP POST
    if (req.method === 'POST' && parsedPath === '/api/telemetry') {
        let body = '';
        req.on('data', chunk => { body += chunk.toString(); });
        req.on('end', () => {
            try {
                const payload = JSON.parse(body);
                console.log('📡 Direct HTTP Telemetry Received:', payload);
                broadcastTelemetry(JSON.stringify(payload));
                
                const shouldReset = resetPending;
                resetPending = false; // Clear reset flag immediately

                res.writeHead(200, { 'Content-Type': 'application/json', 'Access-Control-Allow-Origin': '*' });
                res.end(JSON.stringify({ status: 'ok', reset: shouldReset }));
            } catch (err) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'Invalid JSON' }));
            }
        });
        return;
    }

    if (parsedPath === '/api/reset') {
        resetPending = true;
        console.log('🔄 Reset requested via HTTP API -> resetPending set to true');
        aedes.publish({ topic: 'cit/lift/command', payload: JSON.stringify({ command: "RESET_TO_GROUND" }) });
        res.writeHead(200, { 'Content-Type': 'application/json', 'Access-Control-Allow-Origin': '*' });
        return res.end(JSON.stringify({ status: 'ok', reset: true }));
    }

    if (parsedPath === '/' || parsedPath === '') parsedPath = '/index.html';
    let filePath = path.join(PUBLIC_DIR, parsedPath);

    if (!filePath.startsWith(PUBLIC_DIR)) {
        res.writeHead(403, { 'Content-Type': 'text/html' });
        return res.end('<h1>403 Forbidden</h1>', 'utf-8');
    }

    let extname = String(path.extname(filePath)).toLowerCase();
    let contentType = MIME_TYPES[extname] || 'text/html; charset=utf-8';

    const headers = {
        'Content-Type': contentType,
        'Cache-Control': 'no-store, no-cache, must-revalidate',
        'Pragma': 'no-cache',
        'Expires': '0',
        'Access-Control-Allow-Origin': '*'
    };

    fs.readFile(filePath, (error, content) => {
        if (error) {
            fs.readFile(path.join(PUBLIC_DIR, 'index.html'), (fallbackErr, fallbackContent) => {
                if (fallbackErr) {
                    res.writeHead(404, headers);
                    res.end('<h1>404 Not Found</h1>', 'utf-8');
                } else {
                    res.writeHead(200, headers);
                    res.end(fallbackContent, 'utf-8');
                }
            });
        } else {
            res.writeHead(200, headers);
            res.end(content, 'utf-8');
        }
    });
});

// 3. Setup WebSocket Server for Live Dashboard Broadcasts
const wss = new WebSocket.Server({ server });

function broadcastTelemetry(dataString) {
    wss.clients.forEach(wsClient => {
        if (wsClient.readyState === WebSocket.OPEN) {
            wsClient.send(dataString);
        }
    });
}

wss.on('connection', ws => {
    console.log('💻 Web Dashboard client connected via WebSocket');
    
    // Auto-trigger hardware reset to Ground Floor whenever page is loaded/refreshed
    aedes.publish({ topic: 'cit/lift/command', payload: JSON.stringify({ command: "RESET_TO_GROUND" }) }, () => {
        console.log('🔄 Published RESET_TO_GROUND command to ESP32 on page refresh');
    });

    ws.on('message', (msg) => {
        try {
            const data = JSON.parse(msg);
            if (data.command === 'RESET_TO_GROUND') {
                aedes.publish({ topic: 'cit/lift/command', payload: JSON.stringify(data) });
            }
        } catch(e) {}
    });
});

server.listen(PORT, () => {
    console.log(`=======================================================`);
    console.log(` CIT Library Block Lift Dashboard LIVE!`);
    console.log(` Dashboard URL       : http://localhost:${PORT}`);
    console.log(` Local ESP32 Broker : 192.168.181.98:${MQTT_PORT}`);
    console.log(` Topic               : cit/lift/telemetry`);
    console.log(`=======================================================`);
});
