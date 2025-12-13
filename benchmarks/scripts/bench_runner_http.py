import subprocess
import sys
import statistics
import time

def run_bench(cmd):
    """Runs the node command and parses the 6-line output."""
    try:
        # Run node process
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        lines = result.stdout.strip().split('\n')

        # We expect exactly 6 lines of numbers
        if len(lines) < 6:
            print(f"Error: Unexpected output format: {lines}")
            return None

        return {
            "req": int(lines[0]),
            "err": int(lines[1]),
            "rps": float(lines[2]),
            "p50": float(lines[3]),
            "p90": float(lines[4]),
            "p99": float(lines[5]),
        }
    except subprocess.CalledProcessError as e:
        print(f"Error running benchmark: {e}")
        return None
    except ValueError:
        print("Error parsing output numbers")
        return None

def main():
    if len(sys.argv) < 7:
        print("Usage: python bench_runner_http.py <type> <url> <duration> <concurrency> <runs> <extra_arg>")
        print("  Types: static (requires path as extra_arg), post (requires size_kb as extra_arg)")
        sys.exit(1)

    bench_type = sys.argv[1]
    url = sys.argv[2]
    duration = sys.argv[3]
    concurrency = sys.argv[4]
    runs = int(sys.argv[5])
    extra_arg = sys.argv[6] # path for static, size_kb for post

    cmd = []
    if bench_type == "static":
        # node bench_static.js <url> <path> <duration> <concurrency>
        cmd = ["node", "js/bench_http_static.js", url, extra_arg, duration, concurrency]
    elif bench_type == "post":
        # node bench_post.js <url> <sizeKB> <duration> <concurrency> <keepalive=1>
        cmd = ["node", "js/bench_http_post.js", url, extra_arg, duration, concurrency, "1"]
    else:
        print(f"Unknown type: {bench_type}")
        sys.exit(1)

    print(f"--- Starting HTTP {bench_type.upper()} Benchmark ---")
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
        time.sleep(1) # Cooldown

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