# Minimal HTTP GET Tester (Step 2 — receive-only)

This tiny tool sends **multiple simple `GET` requests** to your server and prints one log line per request using **pipe-separated** fields (`|`).  
It’s designed for **Step 2** of your C HTTP server (accept + read). No fuzzing, no slowloris, no pipelining — just basic HTTP.

> If your Step 2 server only **reads** and doesn’t write a response yet, you’ll typically see `silent` or `closed`. That’s OK.

---

## Files

- `http_tester.py` — the test runner

---

## Requirements

- Python **3.7+**
- No external packages

---

## Quick start

```bash
# 50 requests, 10 concurrent, GET /
python3 http_tester.py --host 127.0.0.1 --port 8080
```

More examples:

```bash
# Heavier but still simple: 200 requests, 20 concurrent
python3 http_tester.py --host 127.0.0.1 --port 8080 --requests 200 --concurrency 20

# Custom path and Host header
python3 http_tester.py --host 127.0.0.1 --port 8080 --path /hello --host-header localhost

# Increase socket timeout and read limit (if the server responds slowly or with larger bodies)
python3 http_tester.py --host 127.0.0.1 --port 8080 --timeout 5 --max-bytes 262144
```

> On Windows, use `python` instead of `python3` if that’s your command.

---

## Output format

The tester prints a header, then one **pipe-separated** line per completed request:

```
id|outcome|status|bytes|ttfb_ms|latency_ms
1|responded|200|1234|8|10
2|silent|||0||2001
3|closed|||0||2
...
```

- **outcome**
  - `responded` — server sent some bytes
  - `silent` — no bytes received (normal for a receive-only server)
  - `closed` — server closed without sending data
  - `timeout` — no data within `--timeout`
  - `refused` — connect failed (listener down / wrong port)
  - `error:*` — client-side error type (rare)
- **status** — HTTP status code if the first line looked like `HTTP/1.1 200 OK`
- **bytes** — total bytes read from the server
- **ttfb_ms** — time-to-first-byte (only shown when any data arrived)
- **latency_ms** — total time until the request finished

A short, **prettier** summary is printed at the end, for example:
```
Summary
───────
responded : 47
silent    : 3
avg_latency_ms: 12
```

---

## CLI options

| Flag              | Default      | Description                                  |
|-------------------|-------------:|----------------------------------------------|
| `--host`          | `127.0.0.1`  | Server IP/host                               |
| `--port`          | `8080`       | Server port                                  |
| `--path`          | `/`          | Request path                                 |
| `--requests`      | `50`         | Total requests to send                       |
| `--concurrency`   | `10`         | How many are in flight at once               |
| `--timeout`       | `2.0`        | Socket connect/recv timeout (seconds)        |
| `--max-bytes`     | `65536`      | Max bytes to read per request                |
| `--host-header`   | *(empty)*    | Value for the `Host:` header (defaults to `--host`) |

> The tester uses `Connection: close` to keep Step 2 behavior simple.

---

## Interpreting results (Step 2)

- **Receive-only server (no write yet)** → expect mostly `silent` or `closed`.
- **Server writes a minimal HTTP response** → expect `responded` lines with status codes.
- **Frequent `timeout`** under load → check accept loop, read timeouts, and non-blocking I/O.
- **`refused`** → server not listening on that host/port.

---

## Troubleshooting

- **`refused`**: confirm your server is running and the port is correct. Try `telnet 127.0.0.1 8080` or `nc 127.0.0.1 8080`.
- **All `timeout`**: increase `--timeout` (e.g., `--timeout 5`), or verify firewall rules.
- **Mixed IPv4/IPv6**: if your server binds only IPv6, try `--host ::1`; if only IPv4, use `--host 127.0.0.1`.

---

## Tips while testing your server

- Enable `SO_REUSEADDR` so you can restart quickly while iterating.
- Add header read timeouts to avoid hanging connections.
- Keep per-connection memory bounded.
- Start simple; switch to non-blocking + `epoll` (level-triggered) later.
