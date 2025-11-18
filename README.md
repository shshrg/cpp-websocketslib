# C++ WebSockets Library

## Overview
This project is an asynchronous high-performance library that supports HTTP(S) and Websocket protocols.


## Features
- Asynchronous I/O based on `asio::awaitable`
- HTTP request and WS Frame handling (headers, body)
- Per-connection and global cancellation
- Graceful shutdown
- Serve Static
- TLS Support


## Build & Run


Generate necessary certificates (if OPENSSL)
```shell
./scripts/generate_cert.sh
```

With OPENSSL
```shell
./compile.sh -o
```

With OPENSSL
```shell
./compile.sh -o --no-openssl
```


Run a basic http server
```shell
./bin/example_http
```

Run a basic https server
```shell
./bin/example_http --config examples/configs/https_basic.conf
```

Run a basic ws server
```shell
./bin/example_ws
```

Run a basic wss server
```shell
./bin/example_ws --config examples/configs/wss_basic.conf
```

Config File Structure:
- [server] - define base address, port, number of threads, ssl usage and default root for static content
- [ssl] - path to certificates
- [static] - set up mounting paths
