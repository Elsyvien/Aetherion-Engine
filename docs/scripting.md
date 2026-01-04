# Scripting

Files:
- `Engine/Scripting/include/Aetherion/Scripting/ScriptingPlaceholder.h`
- `Engine/Scripting/src/ScriptingPlaceholder.cpp`

## Current State

`ScriptingRuntimeStub` is a placeholder with Initialize/Shutdown stubs.
No scripting runtime is wired yet.

---

## Python Runtime Plan

The scripting subsystem will embed Python via pybind11 to enable rapid
iteration and AI-assisted behavior authoring.

### Phase 1: Minimal Embedding

**Goal**: Python interpreter running inside the engine.

1. **Add pybind11 dependency** via CMake FetchContent or submodule.
2. **Initialize Python** in `ScriptingRuntime::Initialize()`:
   ```cpp
   py::scoped_interpreter guard{};
   ```
3. **Expose logging** so Python scripts can use `aetherion.log()`.
4. **Execute scripts** from file path or string.
5. **Shutdown** releases the interpreter.

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

**Goal**: Component-attached scripts with lifecycle hooks.

- `on_start()` – called once when entity enters scene.
- `on_update(dt)` – called each tick.
- `on_destroy()` – called before entity removal.
- `on_collision(other)` – called on physics contact.

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
- [ ] pybind11 integrated in CMake
- [ ] Python interpreter lifecycle
- [ ] Basic logging and script execution
- [ ] Entity/component bindings
- [ ] Behavior script hooks
- [ ] Hot reload
- [ ] LLM generation backend (stub)
