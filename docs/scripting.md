# Scripting

Files:
- `Engine/Scripting/include/Aetherion/Scripting/ScriptingPlaceholder.h`
- `Engine/Scripting/src/ScriptingPlaceholder.cpp`

## Current State

`ScriptingRuntimeStub` supports prompt-to-script generation and optional Python
execution when built with `AETHERION_ENABLE_PYTHON=ON`.

---

## Python Runtime Plan

The scripting subsystem embeds Python (C API today; pybind11 later) to enable
rapid iteration and AI-assisted behavior authoring.

### Phase 1: Minimal Embedding

**Goal**: Python interpreter running inside the engine.

1. **Enable Python** via `-DAETHERION_ENABLE_PYTHON=ON`.
2. **Initialize Python** in `ScriptingRuntime` (lazy on first execution).
3. **Execute scripts** from generated files or in-memory strings.
4. **Expose runtime context** as a Python dict.
5. **Shutdown** releases script caches.

### Phase 2: ECS API Surface

**Goal**: Python can query and modify entities/components.

| Python API                        | C++ Binding                          |
|-----------------------------------|--------------------------------------|
| `aetherion.get_entity(id)`        | Returns EntityHandle wrapper         |
| `entity.get_component("Transform")` | Returns component data as dict     |
| `entity.set_component("Transform", data)` | Updates component         |
| `aetherion.spawn_entity(name)`    | Creates new entity, returns handle   |
| `aetherion.destroy_entity(id)`    | Queues entity for destruction        |
| `aetherion.find_entities_by_tag(tag)` | Returns list of EntityHandles   |

### Phase 3: Behavior Scripts

**Goal**: Component-attached scripts with decision hooks.

- `update(entity, context)` – called on decision tick.
  - `entity`: reserved for future bindings (currently `None`).
  - `context`: dict parsed from JSON (personality, knowledge, context).
  - return `{ "state": "...", "reason": "..." }`.

### Phase 4: Hot Reload

**Goal**: Edit Python files, see changes without restart.

- File watcher monitors `assets/scripts/`.
- On change: re-import module, rebind to entities.
- Error handling: log exceptions, keep last-good version.

### Phase 5: AI-Assisted Authoring

**Goal**: Generate behavior scripts from natural language prompts.

- "Behavior prompt" asset type containing the prompt text.
- On prompt change: call LLM backend → generate Python script.
- Script stored alongside prompt for version control.
- Guardrails: static analysis, sandbox execution, test coverage.

### Implementation Checklist
- [ ] pybind11 integrated in CMake (optional)
- [x] Python interpreter lifecycle
- [x] Basic script execution
- [ ] Entity/component bindings
- [ ] Behavior script hooks
- [ ] Hot reload
- [ ] LLM generation backend (stub)
