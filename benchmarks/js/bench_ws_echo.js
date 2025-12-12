#!/usr/bin/env node
// Usage:
//   node bench_ws_echo.js ws://127.0.0.1:9001/ws-echo [msgBytes] [durationSec] [concurrency]
//
// Example:
//   node bench_ws_echo.js ws://127.0.0.1:9001/ws-echo 100 10 50

const WebSocket = require("ws");

if (process.argv.length < 3) {
    process.exit(1);
}

const url = process.argv[2];
const msgBytes = Number(process.argv[3] || "100");      // payload size in bytes
const durationSec = Number(process.argv[4] || "10");    // measurement window
const concurrency = Number(process.argv[5] || "10");    // number of concurrent connections

const payload = "x".repeat(msgBytes);

let totalRequests = 0;
let errors = 0;
const latencies = [];
let stop = false;

function nowMs() {
    const [sec, ns] = process.hrtime();
    return sec * 1000 + ns / 1e6;
}

async function runClient(id) {
    return new Promise((resolve) => {
        const ws = new WebSocket(url);

        let waitingForEcho = false;
        let sendTimestamp = 0;

        ws.on("open", () => {
            send();
        });

        ws.on("message", () => {
            if (!waitingForEcho) {
                return;
            }
            const rtt = nowMs() - sendTimestamp;
            latencies.push(rtt);
            totalRequests++;

            waitingForEcho = false;
            if (!stop) {
                send();
            } else {
                ws.close();
            }
        });

        ws.on("error", (err) => {
            errors++;
        });

        ws.on("close", () => {
            resolve();
        });

        function send() {
            if (ws.readyState === WebSocket.OPEN) {
                waitingForEcho = true;
                sendTimestamp = nowMs();
                ws.send(payload);
            }
        }
    });
}

const clients = [];
for (let i = 0; i < concurrency; ++i) clients.push(runClient());

setTimeout(() => { stop = true; }, durationSec * 1000);

(async () => {
    const start = nowMs();
    await Promise.all(clients);
    const elapsed = (nowMs() - start) / 1000;

    const sorted = latencies.slice().sort((a, b) => a - b);
    const p = (x) => sorted.length > 0 ? sorted[Math.floor((sorted.length - 1) * x)] : 0;
    const rps = totalRequests / elapsed;

    console.log(totalRequests);
    console.log(errors);
    console.log(rps.toFixed(1));
    console.log(p(0.5).toFixed(2));
    console.log(p(0.9).toFixed(2));
    console.log(p(0.99).toFixed(2));
})();
