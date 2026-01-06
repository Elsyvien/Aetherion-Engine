#include "Aetherion/Scripting/ModuleLoader.h"
#include "Aetherion/Scene/Scene.h"
#include <iostream>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace Aetherion::Scripting
{

ModuleLoader::ModuleLoader() = default;

ModuleLoader::~ModuleLoader()
{
    // Unload all modules
    auto moduleIds = GetLoadedModules();
    for (const auto& id : moduleIds)
    {
        UnloadModule(id);
    }
}

void* ModuleLoader::LoadLibraryImpl(const std::filesystem::path& path)
{
#ifdef _WIN32
    return static_cast<void*>(::LoadLibraryW(path.c_str()));
#else
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void ModuleLoader::UnloadLibrary(void* handle)
{
    if (!handle) return;

#ifdef _WIN32
    ::FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

void* ModuleLoader::GetSymbol(void* handle, const char* name)
{
    if (!handle) return nullptr;

#ifdef _WIN32
    return reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(handle), name));
#else
    return dlsym(handle, name);
#endif
}

std::string ModuleLoader::LoadModule(const std::filesystem::path& libraryPath,
                                     const std::string& moduleName)
{
    if (!std::filesystem::exists(libraryPath))
    {
        std::cerr << "ModuleLoader: Library not found: " << libraryPath << std::endl;
        return "";
    }

    // Load the dynamic library
    void* handle = LoadLibraryImpl(libraryPath);
    if (!handle)
    {
#ifdef _WIN32
        DWORD error = GetLastError();
        std::cerr << "ModuleLoader: Failed to load library: " << libraryPath 
                  << " (Error: " << error << ")" << std::endl;
#else
        std::cerr << "ModuleLoader: Failed to load library: " << libraryPath 
                  << " (" << dlerror() << ")" << std::endl;
#endif
        return "";
    }

    // Get the entry points
    auto createFn = reinterpret_cast<CreateGameModuleFn>(GetSymbol(handle, "CreateGameModule"));
    auto destroyFn = reinterpret_cast<DestroyGameModuleFn>(GetSymbol(handle, "DestroyGameModule"));

    if (!createFn)
    {
        std::cerr << "ModuleLoader: CreateGameModule not found in " << libraryPath << std::endl;
        UnloadLibrary(handle);
        return "";
    }

    // Create the module instance
    IGameModule* module = createFn();
    if (!module)
    {
        std::cerr << "ModuleLoader: Failed to create module instance" << std::endl;
        UnloadLibrary(handle);
        return "";
    }

    // Generate module ID
    std::string moduleId = moduleName.empty() ? 
        libraryPath.stem().string() : moduleName;

    // Check for conflicts
    if (m_modules.find(moduleId) != m_modules.end())
    {
        // Unload existing module first
        UnloadModule(moduleId);
    }

    // Initialize module
    if (m_sceneContext)
    {
        module->OnLoad(m_sceneContext);
    }

    // Store module handle
    ModuleHandle moduleHandle;
    moduleHandle.libraryHandle = handle;
    moduleHandle.moduleInstance = module;
    moduleHandle.createFn = createFn;
    moduleHandle.destroyFn = destroyFn;
    moduleHandle.metadata.name = moduleId;
    moduleHandle.metadata.libraryPath = libraryPath.string();
    moduleHandle.metadata.version = module->GetModuleVersion();
    moduleHandle.metadata.loaded = true;
    moduleHandle.metadata.lastModified = std::filesystem::last_write_time(libraryPath).time_since_epoch().count();

    m_modules[moduleId] = std::move(moduleHandle);

    std::cout << "ModuleLoader: Loaded module '" << moduleId << "' from " << libraryPath << std::endl;
    return moduleId;
}

bool ModuleLoader::UnloadModule(const std::string& moduleId)
{
    auto it = m_modules.find(moduleId);
    if (it == m_modules.end())
    {
        return false;
    }

    auto& handle = it->second;

    // Call OnUnload
    if (handle.moduleInstance)
    {
        handle.moduleInstance->OnUnload();

        // Destroy the instance
        if (handle.destroyFn)
        {
            handle.destroyFn(handle.moduleInstance);
        }
        else
        {
            delete handle.moduleInstance;
        }
    }

    // Unload the library
    UnloadLibrary(handle.libraryHandle);

    m_modules.erase(it);

    std::cout << "ModuleLoader: Unloaded module '" << moduleId << "'" << std::endl;
    return true;
}

bool ModuleLoader::ReloadModule(const std::string& moduleId)
{
    auto it = m_modules.find(moduleId);
    if (it == m_modules.end())
    {
        return false;
    }

    auto libraryPath = it->second.metadata.libraryPath;
    auto moduleName = it->second.metadata.name;

    if (!UnloadModule(moduleId))
    {
        return false;
    }

    std::string newId = LoadModule(libraryPath, moduleName);
    return !newId.empty();
}

IGameModule* ModuleLoader::GetModule(const std::string& moduleId) const
{
    auto it = m_modules.find(moduleId);
    if (it == m_modules.end())
    {
        return nullptr;
    }
    return it->second.moduleInstance;
}

const ModuleMetadata* ModuleLoader::GetMetadata(const std::string& moduleId) const
{
    auto it = m_modules.find(moduleId);
    if (it == m_modules.end())
    {
        return nullptr;
    }
    return &it->second.metadata;
}

std::vector<std::string> ModuleLoader::GetLoadedModules() const
{
    std::vector<std::string> ids;
    ids.reserve(m_modules.size());
    for (const auto& [id, _] : m_modules)
    {
        ids.push_back(id);
    }
    return ids;
}

bool ModuleLoader::IsModuleLoaded(const std::string& moduleId) const
{
    return m_modules.find(moduleId) != m_modules.end();
}

void ModuleLoader::SetSceneContext(Scene::Scene* scene)
{
    m_sceneContext = scene;
}

} // namespace Aetherion::Scripting
