# C++ WebSockets Library

## Overview
This project is an asynchronous multithreaded high-performance library that supports HTTP(S) and Websocket protocols.

### Documentation
https://hhafiya.github.io/documentation/index.html

## Features
- **Asynchronous I/O** using `asio::awaitable`
- HTTP request handling (headers and body)
- WebSocket frame support (text, binary, ping/pong, close)
- Per-connection and global cancellation
- Graceful server shutdown
- Static file serving
- TLS/SSL support for secure connections
- Optional OpenSSL integration


## Build & Run

The project includes a `compile.sh` script with multiple options for building the library, examples, benchmarks, and tests.

### Available build options

| Option                       | Description |
|-------------------------------|-------------|
| `-d, --debug`                 | Build Debug (default) |
| `-o, --release`               | Build Release |
| `--openssl`                   | Build with OpenSSL (default) |
| `--no-openssl`                | Build without OpenSSL |
| `--no-examples`               | Do not build any examples |
| `--no-examples-server`        | Do not build server examples |
| `--no-examples-client`        | Do not build client examples |
| `--bench`                     | Build benchmarks (CPU + WebSocket) |
| `--no-bench`                  | Do not build benchmarks (default) |
| `--tests`                     | Build unit tests (default) |
| `--no-tests`                  | Do not build tests |
| `-c, --clean`                 | Remove build directories |
| `-h, --help`                  | Show help message |

### Build examples

Generate necessary certificates (if OPENSSL)
```shell
./scripts/generate_cert.sh
```

### Server examples

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
./bin/example_ws --config examples/configs/http_basic.conf
```

### Clients examples
Run a http client
```shell
./bin/example_http_client
```

Run a https client
```shell
./bin/example_http_client --config examples/configs/https_basic.conf
```

Run a ws client
```shell
./bin/example_ws_client
```

Run a wss client
```shell
./bin/example_ws_client --config examples/configs/wss_basic.conf
```

Config File Structure:
- [server] - define base address, port, number of threads, ssl usage and default root for static content
- [ssl] - path to certificates
- [static] - set up mounting paths
