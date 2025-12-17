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

```bash
cmake -S . -B build
cmake --build build
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
