# Aetherion Engine Technical Docs

This folder documents the C++ engine/editor code in `Engine/`.
Use these docs as developer-facing references for current behavior and APIs.

## Contents
- engine-overview.md
- core.md
- runtime.md
- scene.md
- assets.md
- rendering.md
- physics.md
- audio.md
- editor.md
- platform.md
- scripting.md
- asset-pipeline.md (existing notes)
- website-alignment-plan.md

---

## Website Alignment Status

See [website-alignment-plan.md](website-alignment-plan.md) for the full gap
analysis between public website claims and current engine capabilities.

### Quick Status Summary

| Subsystem   | Status       | Notes                                    |
|-------------|--------------|------------------------------------------|
| Rendering   | ⚠️ Partial   | Vulkan works; no neural/AI features yet  |
| Assets      | ✅ Functional | GUID registry, hot-reload, cook script   |
| Physics     | ⚠️ Partial   | Jolt works; missing exit events, debug   |
| Audio       | ⚠️ Minimal   | One-shot only; no streaming/spatial      |
| Scripting   | ❌ Placeholder | No runtime; Python plan documented      |
| Platform    | ❌ Placeholder | No OS integration; milestones listed    |
| AI Runtime  | ❌ Stub      | Stubs only; no inference loop            |

### Documentation Updates Completed

- [x] engine-overview.md: Added "Current Capabilities vs Vision" table
- [x] feature-plan.md: Split into MVP and Visionary sections with dependencies
- [x] rendering.md: Added "Renderer Reality" and "Roadmap Hooks"
- [x] asset-pipeline.md: Added sRGB fix plan and virtual assets stub
- [x] scripting.md: Added Python Runtime Plan
- [x] platform.md: Added windowing/input milestone list
- [x] audio.md: Added MVP Audio roadmap
- [x] physics.md: Documented exit events and debug visualization gaps

## Repository Hierarchy (Relevant)
```text
/Aetherion-Engine
├─ /Engine
│  ├─ /Core
│  ├─ /Runtime
│  ├─ /Editor
│  ├─ /Scene
│  ├─ /Assets
│  ├─ /Platform
│  ├─ /Rendering
│  ├─ /Physics
│  ├─ /Audio
│  └─ /Scripting
└─ /docs
```
