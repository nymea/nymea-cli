# nymea-cli

`nymea-cli` is a Qt5/Qt6 + FTXUI terminal client for `nymead`.
It connects to the nymea JSON-RPC API over plain TCP or SSL/TLS and renders a fullscreen terminal UI for browsing things, states, params, and actions.

## Stack

- Qt5 or Qt6 Core + Network
- FTXUI
- CMake
- Generated Qt/C++ API model from nymea `api.json`

## Current features

- Terminal-only application using `QCoreApplication`
- Plain TCP and SSL/TLS connections
- nymea default ports:
  - plain TCP: `2223`
  - SSL/TLS: `2222`
- `JSONRPC.Hello` handshake with server name, server version, and API version in the header
- Authentication via:
  - stored token
  - interactive username/password form
  - `--username` / `--password`
- Persistent connection settings and tokens:
  - `/var/lib/nymea/nymea-cli.conf` when running as root
  - `~/.config/nymea/nymea-cli.conf` otherwise
- TLS fingerprint persistence and warning when the fingerprint changes
- Generated API model from `api/api.json`
- Thing browser with:
  - thing list
  - thing overview
  - params
  - states
  - actions
- Typed rendering for values:
  - booleans with colored indicator
  - color values with hex text and color swatch
  - compact unit rendering such as `ms`, `°C`, `%`, `kWh`
- Action execution dialog:
  - browse actions in the thing view
  - inspect action metadata with `Space`
  - execute actions with `Enter`
  - bool and `allowedValues` params use selector-style input

## Build

```bash
cmake -S . -B build
cmake --build build
```

To force a specific Qt version, point CMake at that installation explicitly. Examples:

```bash
cmake -S . -B build-qt5 -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake/Qt5
cmake --build build-qt5

cmake -S . -B build-qt6 -DCMAKE_PREFIX_PATH=/usr/local/Qt/6.10.1/gcc_64
cmake --build build-qt6
```

## Run

```bash
./build/nymea-cli
```

## Command line

```bash
./build/nymea-cli --help
./build/nymea-cli --version
./build/nymea-cli --host 127.0.0.1 --timeout 5000
./build/nymea-cli --ssl --host 127.0.0.1
./build/nymea-cli --ssl --port 2222
./build/nymea-cli --username admin --password secret
```

## UI navigation

Global:

- `q` or `Esc`: quit
- `c`: reconnect
- `h`: rerun handshake
- `t`: refresh things

Main layout:

- `Up` / `Down`: move in the focused list
- `Left` / `Right`: switch focus between panels

Things view:

- Left panel: thing list
- Right panel: overview, params, states, actions
- `Space` on a param, state, or action: open metadata inspector
- `Enter` on an action: open execute dialog

Action dialog:

- `Up` / `Down`: select param
- `Left` / `Right` / `Space`: cycle selector values for bool or `allowedValues` params
- typing: edit free-form params
- `Enter`: execute action
- `Esc`: close dialog

## Settings file

Connections are stored in INI format and keyed by nymea host UUID.
Each connection stores:

- host UUID
- display name
- host
- port
- SSL enabled flag
- auth token
- certificate fingerprint for TLS connections

Stored tokens are reused on reconnect. If the server rejects a token, the token is cleared and login is requested again.

## API generation

The pinned schema is stored in `api/api.json`.
The file format is:

- line 1: API version
- remaining content: JSON payload

Regenerate the API model with:

```bash
./scripts/generate_nymea_api.py \
  --api-json api/api.json \
  --output src/generated/nymeaapigenerated.h
```

This writes the stable aggregate entry points `src/generated/nymeaapigenerated.h`
and `src/generated/nymeaapigenerated.cpp`, plus `src/generated/apiutils.*` and one
header/source pair per generated API class.

Generator rules used by this project:

- generate the exact nymea API object names, aliases, enums, and flags
- prefer value semantics for generated object references
- use indirection only for recursive schema references that require it
- do not hand-edit files in `src/generated/`

## Repository layout

- `main.cpp`: CLI bootstrap and argument parsing
- `src/engine.*`: application orchestration, UI state, navigation, action execution
- `src/thingmanager.*`: thing and thing-class data access helpers on top of the generated API model
- `src/nymeajsonrpcclient.*`: JSON-RPC socket transport
- `src/connectionsettings.*`: persistent settings and token storage
- `scripts/generate_nymea_api.py`: API generator
- `src/generated/`: generated API model files

## Development

Format before finishing changes:

```bash
clang-format -i main.cpp src/*.h src/*.cpp
```
