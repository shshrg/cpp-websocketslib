#!/usr/bin/env python3

import subprocess
import sys
import statistics
import time

def run_bench(cmd):
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        lines = result.stdout.strip().split('\n')

        # Taking the last 6 lines ensures we ignore any potential "[myserver]..."
        # debug logs if you accidently left them, though the JS shouldn't print any.
        lines = lines[-6:]

        if len(lines) < 6:
            return None

        return {
            "req": int(lines[0]),
            "err": int(lines[1]),
            "rps": float(lines[2]),
            "p50": float(lines[3]),
            "p90": float(lines[4]),
            "p99": float(lines[5]),
        }
    except Exception as e:
        print(f"Err: {e}")
        return None

def main():
    if len(sys.argv) < 6:
        print("Usage: python bench_runner_ws.py <url> <msg_bytes> <duration> <concurrency> <runs>")
        sys.exit(1)

    url = sys.argv[1]
    msg_bytes = sys.argv[2]
    duration = sys.argv[3]
    concurrency = sys.argv[4]
    runs = int(sys.argv[5])

    # node bench_ws_echo.js <url> [msgBytes] [duration] [concurrency]
    cmd = ["node", "js/bench_ws_echo.js", url, msg_bytes, duration, concurrency]

    print(f"--- Starting WebSocket Echo Benchmark ---")
    print(f"Config: {runs} runs, {duration}s each, {concurrency} conn")

    stats = {k: [] for k in ["req", "err", "rps", "p50", "p90", "p99"]}

    for i in range(runs):
        print(f"Run {i+1}/{runs}...", end=" ", flush=True)
        res = run_bench(cmd)
        if res:
            print(f"RPS: {res['rps']}")
            for k, v in res.items():
                stats[k].append(v)
        else:
            print("Failed")
        time.sleep(1)

    print("\n--- Summary ---")
    headers = ["Metric", "Mean", "StdDev"]
    print(f"{headers[0]:<10} {headers[1]:<15} {headers[2]:<15}")
    print("-" * 40)

    for k in ["rps", "p50", "p90", "p99", "req", "err"]:
        data = stats[k]
        if not data: continue
        mean = statistics.mean(data)
        stdev = statistics.stdev(data) if len(data) > 1 else 0.0
        print(f"{k.upper():<10} {mean:<15.2f} {stdev:<15.2f}")

if __name__ == "__main__":
    main()