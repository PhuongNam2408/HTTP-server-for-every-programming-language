#!/usr/bin/env python3
import argparse, socket, time
from concurrent.futures import ThreadPoolExecutor, as_completed

def build_request(path: str, host_header: str, keep_alive: bool=False) -> bytes:
    lines = [
        f"GET {path} HTTP/1.1\r\n",
        f"Host: {host_header}\r\n",
        f"Connection: {'keep-alive' if keep_alive else 'close'}\r\n",
        "\r\n",
    ]
    return "".join(lines).encode("ascii", errors="ignore")

def parse_status_code(data: bytes):
    # Try to read the first line if it looks like "HTTP/1.1 200 OK"
    try:
        head = data.split(b"\r\n", 1)[0].decode("latin1", errors="replace")
        if head.startswith("HTTP/"):
            parts = head.split(" ", 2)
            # parts[1] may be "200"; guard against non-digits
            return int(parts[1]) if len(parts) >= 2 and parts[1].isdigit() else None
    except Exception:
        pass
    return None

def one_get(req_id: int, host: str, port: int, path: str, timeout: float, max_bytes: int, host_header: str):
    t0 = time.monotonic()
    outcome = "unknown"
    status = None
    ttfb_ms = None
    total = 0

    try:
        with socket.create_connection((host, port), timeout=timeout) as s:
            s.settimeout(timeout)
            req = build_request(path, host_header or host, keep_alive=False)
            s.sendall(req)

            # time to first byte
            try:
                first = s.recv(4096)
            except socket.timeout:
                first = b""
            if first:
                ttfb_ms = int((time.monotonic() - t0) * 1000)
                total += len(first)
                status = parse_status_code(first)

            # keep reading until close or limit
            while total < max_bytes:
                try:
                    chunk = s.recv(min(65536, max_bytes - total))
                except socket.timeout:
                    outcome = "timeout"
                    break
                if not chunk:
                    outcome = "closed" if total == 0 else "responded"
                    break
                total += len(chunk)

            if total > 0 and outcome not in ("timeout", "closed"):
                outcome = "responded"
            elif total == 0 and outcome not in ("timeout",):
                outcome = "silent"

    except ConnectionRefusedError:
        outcome = "refused"
    except socket.timeout:
        outcome = "timeout"
    except Exception as e:
        outcome = f"error:{type(e).__name__}"

    latency_ms = int((time.monotonic() - t0) * 1000)
    return {
        "id": req_id,
        "outcome": outcome,
        "status": status,
        "bytes": total,
        "ttfb_ms": ttfb_ms,
        "latency_ms": latency_ms,
    }

def print_banner():
    line = "─" * 62
    print(f"┌{line}┐")
    print("│ Minimal HTTP GET Tester — pipe-separated logs (Step 2) │")
    print(f"└{line}┘")
    print("Fields: id|outcome|status|bytes|ttfb_ms|latency_ms")
    print()

def main():
    ap = argparse.ArgumentParser(description="Minimal multi-GET tester with pipe-separated logs")
    ap.add_argument("--host", default="127.0.0.1", help="Server host/IP")
    ap.add_argument("--port", type=int, default=8080, help="Server port")
    ap.add_argument("--path", default="/", help="Request path")
    ap.add_argument("--requests", type=int, default=50, help="Total number of GET requests")
    ap.add_argument("--concurrency", type=int, default=10, help="How many in parallel")
    ap.add_argument("--timeout", type=float, default=2.0, help="Socket timeout (seconds)")
    ap.add_argument("--max-bytes", type=int, default=65536, help="Max bytes to read per request")
    ap.add_argument("--host-header", default="", help='Value for \"Host:\" header (defaults to --host)')
    args = ap.parse_args()

    print_banner()
    # header
    print("id|outcome|status|bytes|ttfb_ms|latency_ms")

    results = []
    with ThreadPoolExecutor(max_workers=max(1, args.concurrency)) as pool:
        futs = [
            pool.submit(one_get, i+1, args.host, args.port, args.path,
                        args.timeout, args.max_bytes, args.host_header)
            for i in range(args.requests)
        ]
        for fut in as_completed(futs):
            r = fut.result()
            results.append(r)
            # pipe-separated (no commas)
            print(f"{r['id']}|{r['outcome']}|{r['status'] or ''}|{r['bytes']}|{r['ttfb_ms'] or ''}|{r['latency_ms']}")

    # pretty summary
    from collections import Counter
    c = Counter(r["outcome"] for r in results)
    avg_latency = sum(r["latency_ms"] for r in results) / len(results) if results else 0

    print("\nSummary")
    print("───────")
    for k, v in sorted(c.items(), key=lambda kv: (-kv[1], kv[0])):
        print(f"{k:10s}: {v}")
    print(f"{'avg_latency_ms':10s}: {int(avg_latency)}")

if __name__ == "__main__":
    main()
