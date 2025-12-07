#!/usr/bin/env node
// Usage:
//   node bench_ws_echo.js ws://127.0.0.1:9001/ws-echo [msgBytes] [durationSec] [concurrency]
//
// Example:
//   node bench_ws_echo.js ws://127.0.0.1:9001/ws-echo 100 10 50

const WebSocket = require("ws");

if (process.argv.length < 3) {
    console.error("Usage: node bench_ws_echo.js ws://host:port/path [msgBytes] [durationSec] [concurrency]");
    process.exit(1);
}

const url = process.argv[2];
const msgBytes = Number(process.argv[3] || "100");      // payload size in bytes
const durationSec = Number(process.argv[4] || "10");    // measurement window
const concurrency = Number(process.argv[5] || "10");    // number of concurrent connections

const payload = "x".repeat(msgBytes);

let totalRequests = 0;
let errors = 0;
const latencies = []; // ms

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
            // Kick off first message
            send();
        });

        ws.on("message", () => {
            if (!waitingForEcho) {
                // Should not happen logically, but just ignore
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
            // console.error(`[client ${id}] error:`, err.message);
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

function percentile(sorted, p) {
    if (sorted.length === 0) return NaN;
    const idx = (sorted.length - 1) * p;
    const lower = Math.floor(idx);
    const upper = Math.ceil(idx);
    if (lower === upper) return sorted[lower];
    const w = idx - lower;
    return sorted[lower] * (1 - w) + sorted[upper] * w;
}

(async () => {
    console.log(`*** WS echo benchmark ***`);
    console.log(`URL:         ${url}`);
    console.log(`Payload:     ${msgBytes} bytes`);
    console.log(`Duration:    ${durationSec} s`);
    console.log(`Concurrency: ${concurrency}`);

    const clients = [];
    for (let i = 0; i < concurrency; ++i) {
        clients.push(runClient(i));
    }

    setTimeout(() => {
        stop = true;
    }, durationSec * 1000);

    const start = nowMs();
    await Promise.all(clients);
    const elapsed = (nowMs() - start) / 1000;

    const sorted = latencies.slice().sort((a, b) => a - b);

    const p50 = percentile(sorted, 0.5);
    const p90 = percentile(sorted, 0.9);
    const p99 = percentile(sorted, 0.99);

    console.log("\nResults:");
    console.log(`Requests: ${totalRequests}`);
    console.log(`Errors:   ${errors}`);
    console.log(`RPS:      ${(totalRequests / elapsed).toFixed(1)}`);
    if (sorted.length > 0) {
        console.log(`p50:      ${p50.toFixed(2)} ms`);
        console.log(`p90:      ${p90.toFixed(2)} ms`);
        console.log(`p99:      ${p99.toFixed(2)} ms`);
    } else {
        console.log(`No successful requests recorded.`);
    }
})();
