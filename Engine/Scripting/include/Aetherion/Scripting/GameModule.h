#pragma once

#include "Aetherion/Core/Types.h"
#include <memory>
#include <string>

namespace Aetherion::Scene
{
    class Scene;
    class Entity;
    class Component;
}

namespace Aetherion::Scripting
{

// ============================================================================
// GameModule Interface
// ============================================================================

/// @brief Base interface for dynamically loaded game modules
class IGameModule
{
public:
    virtual ~IGameModule() = default;

    /// @brief Called when the module is first loaded
    virtual void OnLoad(Scene::Scene* scene) = 0;

    /// @brief Called when the module is about to be unloaded
    virtual void OnUnload() = 0;

    /// @brief Get the module name
    virtual const char* GetModuleName() const = 0;

    /// @brief Get the module version
    virtual uint32_t GetModuleVersion() const = 0;
};

// ============================================================================
// Standard Module Entry Points (extern "C" for C++ modules)
// ============================================================================

/// @brief Standard entry point for creating a module instance
/// Usage in generated code:
/// @code
/// extern "C" AETHERION_EXPORT IGameModule* CreateGameModule()
/// {
///     return new MyBehaviorModule();
/// }
/// @endcode
using CreateGameModuleFn = IGameModule* (*)();

/// @brief Standard entry point for destroying a module instance
using DestroyGameModuleFn = void (*)(IGameModule*);

// ============================================================================
// Module Metadata
// ============================================================================

struct ModuleMetadata
{
    std::string name;
    std::string sourceFile;
    std::string libraryPath;
    uint32_t version{0};
    uint64_t lastModified{0};
    bool loaded{false};
};

} // namespace Aetherion::Scripting

// Export macros for cross-platform DLL/SO exports
#ifdef _WIN32
    #define AETHERION_EXPORT __declspec(dllexport)
#else
    #define AETHERION_EXPORT __attribute__((visibility("default")))
#endif
