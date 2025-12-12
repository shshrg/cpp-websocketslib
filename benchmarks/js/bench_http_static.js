// Usage:
// node bench_static.js http://127.0.0.1:8080 /static/small.txt 20 100
//   BaseURL  PATH           duration seconds   concurrency

const http = require("http");

if (process.argv.length < 6) process.exit(1);

const base = new URL(process.argv[2]);
const path = process.argv[3];
const duration = Number(process.argv[4]);
const concurrency = Number(process.argv[5]);

const agent = new http.Agent({keepAlive: false});

let completed = 0;
let errors = 0;
let lat = [];
let running = true;

function oneRequest() {
    if (!running) return;
    const start = process.hrtime.bigint();

    const req = http.request(
        {
            hostname: base.hostname,
            port: base.port,
            path: path,
            method: "GET",
            agent,
        },
        (res) => {
            res.on("data", () => {
            });
            res.on("end", () => {
                const end = process.hrtime.bigint();
                lat.push(Number(end - start) / 1e6);
                completed++;
                oneRequest();
            });
        }
    );

    req.on("error", () => {
        errors++;
        oneRequest();
    });

    req.end();
}

// Launch workers
for (let i = 0; i < concurrency; i++) oneRequest();

setTimeout(() => {
    running = false;
    lat.sort((a, b) => a - b);
    const p = (x) => lat.length > 0 ? lat[Math.floor((lat.length - 1) * x)] : 0;
    const rps = completed / duration;

    console.log(completed);
    console.log(errors);
    console.log(rps.toFixed(1));
    console.log(p(0.5).toFixed(2));
    console.log(p(0.9).toFixed(2));
    console.log(p(0.99).toFixed(2));

    process.exit(0);
}, duration * 1000);
