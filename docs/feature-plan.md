# Aetherion Feature Plan

This document outlines concrete next steps for the AI-native roadmap and core subsystem completion. Each section lists near-term milestones and integration notes.

## Committed MVP (Next Releases)
### Editor Copilot (Command-Driven)
- Scaffold `AICopilotPanel` as a Qt dock widget and register it with the existing editor dock layout.
- Expose the command system APIs to the panel (entity spawn, transform, delete, selection) through a narrow interface that can be invoked from text prompts.
- Implement a minimal NL-to-command bridge (rule-based first) that parses prompts like “spawn 10 cubes in a grid” into batched `CreateEntity` + `Transform` commands.
- Add telemetry/logging for executed commands and guardrails (dry-run mode, undo stack integration).
- Dependencies: CommandHistory + EditorSelection + Scene serializer.

### Core Subsystems (Placeholders → MVP)
- Platform: Extend `PlatformAbstraction` with window handles, resize events, vsync toggle, and DPI awareness needed by the renderer/editor.
- Audio: Implement basic audio device init and one-shot playback for imported assets; hook into the asset registry for clips.
- Physics: Harden the existing Jolt sync system and add missing contact-exit events.
- Ensure all new APIs are thread-safe and exposed to both runtime and editor as needed.
- Dependencies: EngineContext service wiring + runtime system scheduling.

### Asset Pipeline Polish
- Finalize GUID-based asset IDs: ensure `.asset.json` travels with renames/moves and remains the canonical ID.
- Harden hot-reload: targeted cache invalidation for meshes/textures, refresh asset browser/inspector on change.
- Improve cooking flow (`tools/cook_assets.py`): emit `asset_index.json` with type info, validate missing IDs, and support virtual assets where applicable.
- Add lightweight tests/CLI checks for asset GUID stability and cooking validation.

---

# Part 2: Visionary / Website Roadmap

These features represent the long-term AI-native vision described on the public
website. They require foundational work from Part 1 before implementation.

## Neural Rendering & Baking
- **Depends on**: Stable Vulkan pipeline, asset registry extensions.
- Integrate NeRF-style neural radiance fields for pre-baked lighting.
- Add DLSS/FSR hooks for AI upscaling.
- Implement neural texture compression pipeline.

## In-Editor Asset Generation
- **Depends on**: Generative Assets (virtual asset URIs), generation pipeline.
- Text-to-texture, text-to-mesh generators with caching.
- Editor UX for prompt input, progress, and regeneration.
- Pluggable generator backends (local diffusion, API-based).

## Logic Copilot → C++ Systems
- **Depends on**: Semantic Scripting (Python runtime), AI Copilot Panel.
- Natural language prompts compiled to ECS systems.
- Iterative refinement loop with automated testing.
- Code review and guardrails before C++ emission.

## Smart LODs & Semantic Scene Understanding
- **Depends on**: Asset registry metadata, runtime AI component.
- Automatic LOD generation based on importance metrics.
- Semantic tagging of scene objects for AI queries.
- Runtime scene graph queries (e.g., "find all chairs").

## On-Device AI Inference Loop
- **Depends on**: Runtime AI Component, behavior executor.
- ONNX/llama.cpp integration for local inference.
- Budgeted per-frame inference scheduling.
- Fallback to remote inference when local unavailable.

## Dependency Graph

```text
[Asset Registry Metadata]
        |
        v
[Generative Assets] -----> [In-Editor Asset Gen]
        |
        v
[Semantic Scripting] ----> [Logic Copilot → C++]
        |
        v
[Runtime AI Component] --> [On-Device Inference Loop]
        |                          |
        v                          v
[Smart LODs] <--------- [Semantic Scene Understanding]
```
- Dependencies: AssetRegistry metadata + editor refresh hooks.

## Visionary/Website (Longer-Term)
### Semantic Scripting (Python/pybind11)
- Replace `ScriptingPlaceholder` with an embedded Python runtime (pybind11).
- Define a “behavior prompt” asset type and pipeline: prompt → generated Python script → hot-reload on prompt change.
- Expose ECS hooks (entity, components, events) to Python with a stable API surface.
- Add a minimal generation backend (stubbed LLM call) and a file watcher to recompile/regenerate on edit.
- Dependencies: Scripting runtime + asset type + hot-reload pipeline.

### Runtime AI Component
- Introduce `AIBehaviorComponent` with fields: Personality, KnowledgeBase reference, CurrentContext, and execution mode (local/on-device vs. remote stub).
- Implement a behavior executor that can call a pluggable policy backend (start with a scripted/stub backend; later ONNX/llama.cpp).
- Add ECS system for ticking behaviors, state transitions, and budgeted updates per frame.
- Provide editor inspectors for the component and debug visualization (current state, last decision).
- Dependencies: ECS scheduler + runtime budgeting.

### Generative Assets
- Extend `AssetRegistry` to register “virtual” asset URIs (e.g., `texture://generate/brick_wall`).
- Implement a generation request pipeline with caching to disk and manifest entries so regenerated assets reuse IDs.
- Add editor UX affordances: progress indicator and retry/fail states in the asset browser.
- Define a pluggable generator interface; ship a stub that returns placeholder assets to keep the pipeline testable.
- Dependencies: Virtual asset registry + cooking pipeline + cache invalidation.

### Neural Rendering + Smart LODs (Website Claims)
- Add hooks for neural denoisers/upscalers and dynamic LOD generation.
- Define a performance budget API for inference-heavy passes.
- Dependencies: RenderView extensions + asset streaming + tooling for capture/training.
