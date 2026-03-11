# AGENTS.md

## Project scope

`nymea-cli` is a terminal-only Qt5/Qt6 + FTXUI client for `nymead`.
It connects to nymea over JSON-RPC using plain TCP or SSL/TLS and renders a fullscreen terminal UI for browsing and executing server-side functionality.

## Architecture

- `main.cpp`
  - thin bootstrap
  - command-line parsing
  - creates `QCoreApplication`
  - constructs and runs `nymea::Engine`
- `src/engine.*`
  - top-level application orchestration
  - connection lifecycle
  - hello/authentication flow
  - FTXUI layout, focus, navigation, dialogs
  - thing/action browsing and action execution
- `src/nymeajsonrpcclient.*`
  - newline-delimited JSON-RPC transport
  - `QTcpSocket` / `QSslSocket`
  - auth token handling
  - peer certificate fingerprint access
- `src/thingmanager.*`
  - stores loaded `api::Thing` values
  - stores loaded `api::ThingClass` values
  - resolves `ParamType`, `StateType`, and `ActionType` metadata for UI rendering
- `src/connectionsettings.*`
  - persistent INI-backed storage for saved connections, tokens, and TLS fingerprints
- `scripts/generate_nymea_api.py`
  - generates the Qt/C++ API model from `api/api.json`
- `src/generated/`
  - generated API model files
  - `apiutils.*` contains shared enums, aliases, and method/notification descriptors
  - `nymeaapigenerated.*` are stable aggregate entry points for the generated API
  - every generated API class has its own header/source pair
  - never edit generated files manually

## Repository layout

- `api/api.json`: pinned nymea API schema snapshot; first line is API version, remaining content is JSON
- `build/`: local build directory
- `src/generated/`: generated API code only

## Build

```bash
cmake -S . -B build
cmake --build build
```

Qt Creator build directories may also exist alongside the default `build/` directory. Prefer building the directory already used by the user if the session context makes that obvious.

## Runtime behavior

- Keep the app terminal-only. Do not introduce Qt Widgets or other GUI dependencies.
- Keep JSON-RPC transport newline-delimited compact JSON (`...\n`).
- Keep nymea default ports consistent:
  - `2222` for SSL/TLS
  - `2223` for plain TCP
- Use `/var/lib/nymea/nymea-cli.conf` when running as root.
- Use `~/.config/nymea/nymea-cli.conf` when not running as root.
- Persist saved connections by host UUID.
- Persist auth token and TLS fingerprint per connection.
- Show a clear warning if a stored TLS fingerprint changes.
- Reuse stored tokens across restarts.
- If the server rejects a stored token, clear that token and require authentication again.
- Process `JSONRPC.Hello` replies and expose server version and API version in the UI header.
- If authentication is required, support both interactive login and `--username` / `--password`.

## UI expectations

The current UI is menu-driven and keyboard-only.

Main structure:

- left: main menu
- right: current view
- `Things` view contains:
  - thing list
  - thing overview
  - browsable params
  - browsable states
  - browsable actions

Interaction rules:

- `Up` / `Down` navigate the focused list
- `Left` / `Right` switch focus between panels
- `Space` opens metadata inspector for the selected param, state, or action
- `Enter` on an action opens the execution dialog
- action dialog:
  - `Enter` executes
  - `Esc` closes
  - bool values should be selector-style, not free text
  - `allowedValues` should be selector-style, not free text

When extending the UI, preserve this interaction model unless the user asks for a redesign.

## API model generation rules

The generator is part of the application design, not a loose helper script.

Required behavior:

- Generate the exact nymea API model from `api.json`.
- Use declared API object names, aliases, enums, and flags.
- Do not invent mirrored convenience structs for API objects in app code.
- Prefer value semantics for generated object references and collections.
- Use `QSharedPointer` only where recursive schema references require indirection to compile.
- Preserve CamelCase/lowerCamelCase generated identifiers.
- Avoid underscore-based generated identifiers unless forced by correctness.
- If the schema changes, regenerate instead of patching generated output manually.

Regeneration command:

```bash
./scripts/generate_nymea_api.py --api-json api/api.json --output src/generated/nymeaapigenerated.h
```

After generator changes:

1. Regenerate `src/generated/nymeaapigenerated.h` and its sibling generated files
2. Rebuild the application
3. Fix consuming code to use the new generated model rather than reintroducing hand-written mirrors

## C++ conventions

- Use CamelCase naming in C++ code.
- Name private members with `m_` prefix.
- Use lowercase file names without underscores for class source/header pairs.
  - example: `thingmanager.h` / `thingmanager.cpp`
- Prefer generated API classes directly in consuming code.
- Keep helper abstractions thin and centered on behavior, not duplicate data models.

## Editing rules

- Do not edit generated files in `src/generated/` manually.
- Prefer changing `scripts/generate_nymea_api.py` when generated output is wrong.
- Keep changes ASCII unless the file already contains non-ASCII and the addition is justified.
- Run formatting before finishing C++ changes:

```bash
clang-format -i main.cpp src/*.h src/*.cpp
```

## Verification

For code changes, prefer ending with a real build verification.
Typical command:

```bash
CCACHE_DISABLE=1 /opt/Qt/Tools/CMake/bin/cmake --build build/Desktop_Qt_6_10_2-Debug --target all -j4
```

If the user is using another build directory, adapt accordingly.
