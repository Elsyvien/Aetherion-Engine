# Website Alignment Plan

This plan maps website claims to the current engine state and lists concrete
documentation updates required before implementation work begins.

## Goals
- Align public-facing claims with what the engine actually does today.
- Record the gaps as scoped, testable implementation tasks.
- Add explicit doc sections that track readiness per subsystem.

## Reality Check (Website Claims vs Engine)
- Neural baking / neural rendering: not implemented in renderer.
- Asset generation in-editor: not implemented (no generator pipeline).
- Logic Copilot compiling to C++ systems: not implemented.
- Smart LODs / semantic scene understanding: not implemented.
- Auto texture compression/resizing: not implemented.
- AI-native runtime inference loop: not implemented (stubs only).
- Platform layer with OS windowing/input: placeholder.
- Scripting runtime: placeholder.
- Physics: functional Jolt wrapper, still missing exit events and tooling polish.
- Audio: minimal one-shot playback; no graph/spatialization/streaming.

## Doc Changes (Concrete Edits)
1) docs/engine-overview.md
   - Add a "Current Capabilities vs Vision" section with a short table.
   - Explicitly call out placeholder subsystems (Platform, Scripting).

2) docs/feature-plan.md
   - Split into "Committed MVP" and "Visionary/Website" sections.
   - Add dependencies between items (e.g., Asset Gen depends on Asset Registry
     metadata + generation pipeline).

3) docs/rendering.md
   - Add a "Renderer Reality" section clarifying:
     - No neural baking / NeRF / DLSS integration yet.
     - Current Vulkan passes and limitations (opaque + picking + post-process).
   - Add a "Roadmap Hooks" section that lists required APIs for future AI paths.

4) docs/asset-pipeline.md
- Add "Albedo Texture Fix Plan":
     - Mark albedo textures as `import.srgb = true` in asset metadata.
     - Ensure shader sampling uses sRGB for albedo and linear for data maps.
     - Add a validation check in cook_assets for `import.srgb`.
   - Add "Virtual Assets" stub section aligned with generative assets.

5) docs/scripting.md
   - Replace placeholder note with a "Python Runtime Plan":
     - Minimal embedding plan (pybind11).
     - API surface for entity/component access.

6) docs/platform.md
   - Add a concrete windowing/input milestone list (window creation, resize,
     DPI, swapchain surface handles).

7) docs/audio.md
   - Add a "MVP Audio" section: streaming, spatialization, and editor controls.

8) docs/physics.md
   - Document missing contact exit events and editor debug visualization.

9) docs/README.md
   - Add a "Website Alignment" link to this plan and a short status summary.

## Implementation Readiness Checklist
- Asset registry can represent color space and virtual assets.
- Renderer supports sRGB/linear binding and validation.
- Editor exposes diagnostics for assets and render passes.
- Runtime has a system scheduler and deterministic ordering.
