# Engine Overview

Aetherion is a modular C++20 engine with a Qt 6 editor and a Vulkan renderer.
The repository already contains a working editor and runtime loop; some systems
remain placeholders (audio, scripting, and parts of physics/renderer pipelines
are still evolving).

## Current Capabilities vs Vision
| Area | Current | Website Vision |
| --- | --- | --- |
| Rendering | Vulkan viewport + PBR-ish lighting | Neural baking, neural rendering |
| Assets | Registry + import metadata | Generative/virtual assets |
| Scripting | Placeholder stub | Semantic scripting with Python/LLM |
| Runtime AI | AIBehavior component only | Runtime inference + policy engine |
| Platform | Placeholder abstraction | Full window/input/DPI pipeline |

## Module Map
- Engine/Core: basic types, logging, math, UUIDs.
- Engine/Runtime: EngineApplication main loop, EngineContext service container.
- Engine/Scene: entity/component scene graph and JSON serialization.
- Engine/Assets: asset registry, metadata, import settings, materials.
- Engine/Rendering: VulkanContext + VulkanViewport and RenderView data model.
- Engine/Physics: Jolt-backed PhysicsWorld and a scene sync system.
- Engine/Audio: miniaudio-backed AudioEngine and simple AudioSystem.
- Engine/Editor: Qt widgets for editing, selection, and rendering integration.
- Engine/Platform: minimal window abstraction (placeholder).
- Engine/Scripting: placeholder runtime stub.

## High-Level Data Flow
Editor (Qt) drives the runtime loop and renderer:

1) EditorMainWindow -> EngineApplication::Tick()
2) EngineApplication -> RenderViewSystem rebuilds RenderView from Scene
3) EditorMainWindow -> VulkanViewport::RenderFrame(RenderView)
4) VulkanViewport pulls assets from AssetRegistry caches
5) Editor uses pick results to update EditorSelection

Simulation (play/pause/step) is handled in EngineApplication and propagated
through EngineContext to physics and scene updates.

## Asset and Scene Flow
- AssetRegistry scans the assets root and assigns stable asset IDs via
  per-asset `.asset.json` metadata files.
- SceneSerializer stores scene JSON with entity/component data and asset IDs.
- Runtime loads a bootstrap scene from `assets/scenes/bootstrap_scene.json`
  (or a project-specified path from `aetherion.project.json`).

## Current Capabilities vs Vision

This table summarizes what the engine can do today versus the public roadmap vision.

| Subsystem        | Current State                          | Vision / Roadmap                         |
|------------------|----------------------------------------|------------------------------------------|
| Rendering        | Vulkan passes (opaque, picking, post)  | Neural baking, NeRF, DLSS integration    |
| Asset Pipeline   | GUID registry, hot-reload, cook script | In-editor AI asset generation            |
| Scripting        | Placeholder stub                       | Python runtime + Logic Copilot → C++     |
| Physics          | Jolt wrapper, basic sync               | Full tooling, exit events, debug viz     |
| Audio            | One-shot playback (miniaudio)          | Streaming, spatialization, audio graphs  |
| Platform         | Placeholder descriptor only            | Full windowing, input, DPI, vsync        |
| AI Runtime       | Stubs only                             | On-device inference loop, behavior exec  |
| LOD / Semantics  | Not implemented                        | Smart LODs, semantic scene understanding |

### Placeholder Subsystems
- **Platform**: `PlatformAbstractionLayer` currently stores a `WindowDescriptor`
  but performs no OS integration. Window creation, resize, DPI, and surface
  handles are pending.
- **Scripting**: `ScriptingRuntimeStub` exposes Initialize/Shutdown but has no
  embedded interpreter. Python/pybind11 embedding is planned.
