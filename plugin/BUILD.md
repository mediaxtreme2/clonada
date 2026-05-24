# Clonada Plugin Build Instructions

## Prerequisites

- CMake 3.22+
- C++17 compiler (MSVC 2022, Xcode 14+, GCC 11+)
- ZeroMQ (libzmq) - optional, will build from source if not found

### macOS
```bash
brew install cmake pkg-config zeromq
```

### Windows
```bash
vcpkg install zeromq:x64-windows
```

### Linux (development only)
```bash
sudo apt install cmake build-essential libzmq3-dev libasound2-dev libx11-dev libxrandr-dev libxcursor-dev libfreetype-dev
```

## Build

```bash
cd plugin
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

## Output

Built plugins are in `build/Clonada_artefacts/Release/`:
- `VST3/Clonada.vst3` - VST3 plugin
- `AU/Clonada.component` - Audio Unit (macOS only)
- `Standalone/Clonada.app` or `Clonada.exe` - Standalone app

## Architecture

The plugin is a thin audio shell. It does NOT run AI inference directly:

1. Audio comes in from the DAW on the audio thread
2. Samples are written to a lock-free ring buffer (never blocks)
3. A background thread reads chunks and sends them via ZeroMQ to the Python AI engine
4. The Python engine (clonada_server.py) runs RVC inference and returns converted audio
5. Converted audio is written to the output ring buffer
6. The audio thread reads from the output ring buffer and sends to DAW

This architecture ensures zero audio glitches regardless of AI processing time.
The Python engine must be running separately (started by the installer).
