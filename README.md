# nymea-cli

`nymea-cli` is a CMake-based C++ terminal client for `nymead`, using:

- Qt6 Core + Network for CLI and TCP transport
- FTXUI for fullscreen terminal UI
- JSON-RPC over TCP
- generated Qt/C++ API representation classes from nymea `api.json`

## Features

- Qt6 `QCoreApplication` + `QTcpSocket` / `QSslSocket` (no GUI dependencies)
- FTXUI fullscreen terminal UI
- Connects to nymead via TCP (`--host`, `--port`) or SSL/TLS (`--ssl`)
- Processes `JSONRPC.Hello` and shows server/API versions
- Loads and displays thing list via `Integrations.GetThings`
- Supports login when authentication is required (interactive form or CLI credentials)
- Built-in `--help` and `--version`
- API model generation script: `scripts/generate_nymea_api.py`

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/nymea-cli
```

## CLI options

```bash
./build/nymea-cli --help
./build/nymea-cli --version
./build/nymea-cli --host 127.0.0.1 --timeout 5000          # plain TCP, default port 2223
./build/nymea-cli --ssl --host 127.0.0.1 --timeout 5000    # SSL/TLS, default port 2222
./build/nymea-cli --ssl --port 2222
./build/nymea-cli --username admin --password secret
```

## Generate API model

The generator expects the nymea API schema format where line 1 is the API version
and the remaining file is JSON.

```bash
./scripts/generate_nymea_api.py \
  --api-json api/api.json \
  --output src/generated/nymea_api_generated.h
```
