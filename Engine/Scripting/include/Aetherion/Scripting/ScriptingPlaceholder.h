#pragma once

namespace Aetherion::Scripting
{
class ScriptingRuntimeStub
{
public:
    ScriptingRuntimeStub() = default;
    ~ScriptingRuntimeStub() = default;

    void Initialize() {}
    void Shutdown() {}
};

// ===============================
// TODO: Scripting System Placeholder
// ===============================
// This module will host scripting runtimes (e.g., Lua, Python) and bindings.
// No implementation exists at this stage.

inline void TouchScriptingModule() {}
} // namespace Aetherion::Scripting
