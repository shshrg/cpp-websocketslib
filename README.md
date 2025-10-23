# Web Server with WebSockets Library

## Overview
This project implements a high-performance asynchronous HTTP/1.1 web server with WebSocket support using Asio coroutines


## Features
- Asynchronous I/O based on `asio::awaitable`
- HTTP request parsing (headers, body, query params)
- Per-connection and global cancellation
- Graceful shutdown

## Architecture
- `Server::do_accept()` runs continuously, accepting connections until cancelled.
- Each client connection spawns a `Session`, which reads headers and body; builds a response; sends it back
- Every async operation is cancellable using `asio::cancellation_slot`

## Build & Run
```shell
./compile.sh
```

```
./bin/server_app [port] [threads]
```

```
./bin/client [port]
```
Here:
- `[port]` - port number
- `[threads]` - number of threads