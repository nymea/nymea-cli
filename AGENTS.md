# AGENTS.md

## Project scope

`nymea-cli` is a Qt6 + FTXUI terminal client for `nymead` using JSON-RPC over TCP.

## Repository layout

- `main.cpp`: terminal UI entry point and command-line handling.
- `src/nymea_jsonrpc_client.*`: TCP JSON-RPC transport/client primitives.
- `scripts/generate_nymea_api.py`: generator for Qt/C++ API model types from nymea `api.json`.
- `api/api.json`: pinned API schema snapshot (first line is nymea API version, followed by JSON).
- `src/generated/nymea_api_generated.h`: generated API representation.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## API generation workflow

1. Refresh `api/api.json` from nymea server repo if needed.
2. Regenerate API C++ model:

```bash
./scripts/generate_nymea_api.py --api-json api/api.json --output src/generated/nymea_api_generated.h
```

3. Rebuild and verify the generated header compiles.

If Python is available, CMake also exposes `generate-nymea-api` and runs it as a dependency for `nymea-cli`.

## Conventions

- Keep the app terminal-only: no Qt Widgets/GUI dependencies.
- Keep JSON-RPC transport newline-delimited compact JSON (`...\\n`) as in nymea app/client behavior.
- Keep nymea default ports consistent: `2222` for SSL/TLS and `2223` for plain TCP.
- Process `JSONRPC.Hello` replies and expose server version + server API version in UI header.
- If hello indicates authentication is required, support both interactive login form and `--username` / `--password` CLI flow.
- Prefer adding generated output to `src/generated/` only through the generator script.
- When API schema changes, regenerate instead of editing generated files manually.
- Use CamelCase naming in C++ code (avoid snake_case identifiers except private member `m_` prefixes).
- Name private class members with `m_` prefix.
- Run formatting with repository `.clang-format` before finishing C++ changes:

```bash
clang-format -i main.cpp src/*.h src/*.cpp
```
