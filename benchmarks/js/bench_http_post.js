// Usage:
// node bench_post.js http://127.0.0.1:8080/echo payload_size_kb duration concurrency keepalive

const http = require("http");

const target = new URL(process.argv[2]);
const sizeKB = Number(process.argv[3]);      // e.g. 1, 100, 1000
const duration = Number(process.argv[4]);    // seconds
const concurrency = Number(process.argv[5]); // clients
const keepAlive = process.argv[6] === "1";

const payload = Buffer.alloc(sizeKB * 1024, "A");
let completed = 0;
let errors = 0;
let lat = [];
let running = true;

const agent = new http.Agent({ keepAlive });

function send_one() {
    if (!running) return;
    const start = process.hrtime.bigint();

    const req = http.request(
        {
            hostname: target.hostname,
            port: target.port,
            path: target.pathname,
            method: "POST",
            agent,
            headers: {
                "Content-Type": "application/octet-stream",
                "Content-Length": payload.length,
            },
        },
        (res) => {
            res.on("data", () => {}); // ignore
            res.on("end", () => {
                const ms = Number(process.hrtime.bigint() - start) / 1e6;
                lat.push(ms);
                completed++;
                send_one();
            });
        }
    );

    req.on("error", () => {
        errors++;
        send_one();
    });

    req.write(payload);
    req.end();
}

for (let i = 0; i < concurrency; i++) send_one();

setTimeout(() => {
    running = false;
    lat.sort((a, b) => a - b);
    const p = (x) => lat.length > 0 ? lat[Math.floor(lat.length * x)] : 0;
    const rps = completed / duration;

    console.log(completed);
    console.log(errors);
    console.log(rps.toFixed(1));
    console.log(p(0.5).toFixed(2));
    console.log(p(0.9).toFixed(2));
    console.log(p(0.99).toFixed(2));

    process.exit(0);
}, duration * 1000);
