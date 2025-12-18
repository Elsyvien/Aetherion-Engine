# Aetherion Engine (Scaffolding)

Aetherion is a modular, editor-driven 2D engine scaffold built with modern C++ and Qt 6. This repository only contains architecture and UI shells—no rendering, physics, audio, ECS logic, or asset pipelines are implemented.

## Layout

```
/Aetherion
├─ /Engine
│  ├─ /Core
│  ├─ /Runtime
│  ├─ /Editor
│  ├─ /Scene
│  ├─ /Assets
│  ├─ /Platform
│  ├─ /Rendering    // placeholder only
│  ├─ /Physics      // placeholder only
│  ├─ /Audio        // placeholder only
│  └─ /Scripting    // placeholder only
├─ /CMake
├─ CMakeLists.txt
└─ README.md
```

## Building

This project uses CMake and Qt 6 (Widgets). Below are platform-specific steps.

### Windows (MinGW)

Prerequisites:
- Qt 6 MinGW (e.g. installed under `C:/Qt/6.9.1/mingw_64`)
- MinGW toolchain from Qt Tools (e.g. `C:/Qt/Tools/mingw1310_64`)

Build and run:

```powershell
$env:PATH = "C:\\Qt\\Tools\\mingw1310_64\\bin;C:\\Qt\\6.9.1\\mingw_64\\bin;" + $env:PATH
cmake -S . -B build-mingw -G "MinGW Makefiles" -DQt6_DIR="C:/Qt/6.9.1/mingw_64/lib/cmake/Qt6" -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe"
cmake --build build-mingw -- -j 8
./build-mingw/AetherionEditor.exe
```

### Windows (MSVC)

Install Qt for `msvc2022_64` and use Visual Studio generator:

```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -DQt6_DIR="C:/Qt/6.9.1/msvc2022_64/lib/cmake/Qt6"
cmake --build build-msvc --config Debug
./build-msvc/Debug/AetherionEditor.exe
```

### macOS (Homebrew)

```bash
brew install qt
cmake -S . -B build -DQt6_DIR="$(brew --prefix qt)/lib/cmake/Qt6"
cmake --build build -- -j 8
./build/AetherionEditor
```

The build produces:
- `AetherionRuntime` (static library)
- `AetherionEditor` (Qt 6 Widgets executable)

## Dependencies

- CMake 3.21+ and a C++20 compiler
- Qt 6 (Widgets module) development packages
- **Future rendering work**: Vulkan SDK (headers, loader, validation layers, shader tools like `glslc`/`glslangValidator`; on macOS also MoltenVK)
- Optional future modules: Box2D/Chipmunk2D (physics), OpenAL Soft/SDL_mixer/FMOD/Wwise (audio), Lua/Python (scripting)

## Notes

- All systems are placeholders with TODO markers for future implementation.
- Runtime is editor-agnostic; editor depends on runtime interfaces.
- Rendering/physics/audio/scripting are stub modules for future growth.
