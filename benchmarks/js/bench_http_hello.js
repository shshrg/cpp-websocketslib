// bench_http_hello.js
// Usage:
//   node bench_http_hello.js http://127.0.0.1:8080 /hello 30 100
//     = URL, path, durationSeconds, concurrency

const http = require("http");

if (process.argv.length < 6) {
    console.error("Usage: node bench_http_hello.js <baseUrl> <path> <durationSeconds> <concurrency>");
    process.exit(1);
}

const baseUrl = new URL(process.argv[2]);
const path = process.argv[3];
const durationSeconds = Number(process.argv[4]);
const concurrency = Number(process.argv[5]);

// Keep-alive agent so we don't create a new TCP connection each time
const agent = new http.Agent({
    keepAlive: true,
    maxSockets: concurrency,
});

let totalRequests = 0;
let errorCount = 0;
let latencies = [];
let running = true;

function percentile(arr, p) {
    if (arr.length === 0) return 0;
    const sorted = [...arr].sort((a, b) => a - b);
    const idx = Math.floor((p / 100) * (sorted.length - 1));
    return sorted[idx];
}

function makeRequestLoop(workerId) {
    const options = {
        hostname: baseUrl.hostname,
        port: baseUrl.port || 80,
        path,
        method: "GET",
        agent,
    };

    function oneRequest() {
        if (!running) return;

        const start = process.hrtime.bigint();

        const req = http.request(options, (res) => {
            // We don't care about body content; just drain it.
            res.on("data", () => {});
            res.on("end", () => {
                const end = process.hrtime.bigint();
                const diffNs = Number(end - start); // may overflow after extremely long runs, fine for benchmarks
                const diffMs = diffNs / 1e6;
                latencies.push(diffMs);
                totalRequests++;

                // Immediately send another request
                if (running) {
                    setImmediate(oneRequest);
                }
            });
        });

        req.on("error", (err) => {
            errorCount++;
            console.error("Request error:", err.message);
            if (running) {
                setImmediate(oneRequest);
            }
        });

        req.end();
    }

    // Kick off the first request for this worker
    oneRequest();
}

(async () => {
    console.log(`Benchmarking ${baseUrl.origin}${path}`);
    console.log(`Duration: ${durationSeconds}s, Concurrency: ${concurrency}`);

    const startTime = process.hrtime.bigint();

    // Start N workers
    for (let i = 0; i < concurrency; i++) {
        makeRequestLoop(i);
    }

    // Stop after durationSeconds
    setTimeout(() => {
        running = false;

        const endTime = process.hrtime.bigint();
        const totalNs = Number(endTime - startTime);
        const totalSec = totalNs / 1e9;

        // Give some time for outstanding requests to finish
        setTimeout(() => {
            agent.destroy();

            const rps = totalRequests / totalSec;
            const avgLatency = latencies.length
                ? latencies.reduce((a, b) => a + b, 0) / latencies.length
                : 0;

            console.log("\n=== Results ===");
            console.log(`Total time:     ${totalSec.toFixed(2)} s`);
            console.log(`Total requests: ${totalRequests}`);
            console.log(`Errors:         ${errorCount}`);
            console.log(`RPS:            ${rps.toFixed(2)}`);

            if (latencies.length > 0) {
                console.log(`Latency avg:    ${avgLatency.toFixed(2)} ms`);
                console.log(`Latency p50:    ${percentile(latencies, 50).toFixed(2)} ms`);
                console.log(`Latency p90:    ${percentile(latencies, 90).toFixed(2)} ms`);
                console.log(`Latency p99:    ${percentile(latencies, 99).toFixed(2)} ms`);
            }

            process.exit(0);
        }, 2000);
    }, durationSeconds * 1000);
})();
