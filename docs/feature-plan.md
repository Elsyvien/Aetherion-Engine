# Aetherion Feature Plan

This document outlines concrete next steps for the AI-native roadmap and core subsystem completion. Each section lists near-term milestones and integration notes.

## AI Copilot Panel (Editor)
- Scaffold `AICopilotPanel` as a Qt dock widget and register it with the existing editor dock layout.
- Expose the command system APIs to the panel (entity spawn, transform, delete, selection) through a narrow interface that can be invoked from text prompts.
- Implement a minimal NL-to-command bridge (rule-based first) that parses prompts like “spawn 10 cubes in a grid” into batched `CreateEntity` + `Transform` commands.
- Add telemetry/logging for executed commands and guardrails (dry-run mode, undo stack integration).

## Semantic Scripting (Python/pybind11)
- Replace `ScriptingPlaceholder` with an embedded Python runtime (pybind11).
- Define a “behavior prompt” asset type and pipeline: prompt → generated Python script → hot-reload on prompt change.
- Expose ECS hooks (entity, components, events) to Python with a stable API surface.
- Add a minimal generation backend (stubbed LLM call) and a file watcher to recompile/regenerate on edit.

## Runtime AI Component
- Introduce `AIBehaviorComponent` with fields: Personality, KnowledgeBase reference, CurrentContext, and execution mode (local/on-device vs. remote stub).
- Implement a behavior executor that can call a pluggable policy backend (start with a scripted/stub backend; later ONNX/llama.cpp).
- Add ECS system for ticking behaviors, state transitions, and budgeted updates per frame.
- Provide editor inspectors for the component and debug visualization (current state, last decision).

## Generative Assets
- Extend `AssetRegistry` to register “virtual” asset URIs (e.g., `texture://generate/brick_wall`).
- Implement a generation request pipeline with caching to disk and manifest entries so regenerated assets reuse IDs.
- Add editor UX affordances: progress indicator and retry/fail states in the asset browser.
- Define a pluggable generator interface; ship a stub that returns placeholder assets to keep the pipeline testable.

## Core Subsystems (Placeholders → MVP)
- Physics: Add minimal rigidbody/collider components and a stepping system (start with a simple CPU integrator stub; later swap to a physics library).
- Audio: Implement basic audio device init and one-shot playback for imported assets; hook into the asset registry for clips.
- Platform: Extend `PlatformAbstraction` with window handles, resize events, vsync toggle, and DPI awareness needed by the renderer/editor.
- Ensure all new APIs are thread-safe and exposed to both runtime and editor as needed.

## Asset Pipeline Polish
- Finalize GUID-based asset IDs: ensure `.asset.json` travels with renames/moves and remains the canonical ID.
- Harden hot-reload: targeted cache invalidation for meshes/textures, refresh asset browser/inspector on change.
- Improve cooking flow (`tools/cook_assets.py`): emit `asset_index.json` with type info, validate missing IDs, and support virtual assets where applicable.
- Add lightweight tests/CLI checks for asset GUID stability and cooking validation.
