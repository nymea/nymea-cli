# nymea-cli

Minimal CMake-based C++ console application that uses Qt6 Core for app/CLI
handling and FTXUI for terminal UI rendering.

## Features

- Qt6 `QCoreApplication` (no GUI dependencies)
- FTXUI fullscreen terminal UI
- Top headline rectangle rendered in the terminal
- Application version shown inside the headline bar
- Built-in `--help` / `-h` and `--version` / `-v`

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
```
