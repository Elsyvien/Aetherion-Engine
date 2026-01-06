#pragma once

#include "Aetherion/Scripting/GameModule.h"
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Aetherion::Scripting
{

// ============================================================================
// ModuleLoader - Manages dynamic library loading/unloading
// ============================================================================

class ModuleLoader
{
public:
    ModuleLoader();
    ~ModuleLoader();

    /// @brief Load a module from a dynamic library
    /// @param libraryPath Path to the .dll/.so file
    /// @param moduleName Optional name override
    /// @return Module ID for future reference, or empty string on failure
    std::string LoadModule(const std::filesystem::path& libraryPath,
                          const std::string& moduleName = "");

    /// @brief Unload a module by ID
    /// @return true if successfully unloaded
    bool UnloadModule(const std::string& moduleId);

    /// @brief Reload a module (unload + load)
    bool ReloadModule(const std::string& moduleId);

    /// @brief Get a loaded module instance
    IGameModule* GetModule(const std::string& moduleId) const;

    /// @brief Get metadata for a module
    const ModuleMetadata* GetMetadata(const std::string& moduleId) const;

    /// @brief Get all loaded module IDs
    std::vector<std::string> GetLoadedModules() const;

    /// @brief Check if a module is loaded
    bool IsModuleLoaded(const std::string& moduleId) const;

    /// @brief Set the scene context for newly loaded modules
    void SetSceneContext(Scene::Scene* scene);

private:
    struct ModuleHandle
    {
        void* libraryHandle{nullptr};  // HMODULE on Windows, void* on Unix
        IGameModule* moduleInstance{nullptr};
        ModuleMetadata metadata;
        CreateGameModuleFn createFn{nullptr};
        DestroyGameModuleFn destroyFn{nullptr};
    };

    void* LoadLibraryImpl(const std::filesystem::path& path);
    void UnloadLibrary(void* handle);
    void* GetSymbol(void* handle, const char* name);

    std::unordered_map<std::string, ModuleHandle> m_modules;
    Scene::Scene* m_sceneContext{nullptr};
};

} // namespace Aetherion::Scripting
