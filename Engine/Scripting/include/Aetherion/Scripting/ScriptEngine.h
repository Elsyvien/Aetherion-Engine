#pragma once

#include "Aetherion/Core/Types.h"
#include <string>
#include <memory>

namespace Aetherion::Scripting {

class ScriptInstance;

/// @brief Base class for a scripting engine implementation (e.g. Lua, Python)
class ScriptEngine {
public:
    enum class SourceKind : std::uint8_t {
        InlineCode = 0,
        FilePath = 1,
    };

    virtual ~ScriptEngine() = default;

    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;

    /// @brief Create an instance of a script from a source file or asset
    virtual std::unique_ptr<ScriptInstance>
    CreateInstance(const std::string& scriptSource, SourceKind sourceKind) = 0;

    /// @brief Called every frame to process engine-level scripting tasks
    virtual void OnUpdate(float deltaTime) = 0;
};

} // namespace Aetherion::Scripting
