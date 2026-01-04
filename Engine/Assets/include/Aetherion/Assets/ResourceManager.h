#pragma once

#include "Aetherion/Core/UUID.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <string>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Aetherion::Assets {

// Forward declarations
class IResource;
class ResourceManager;

/// @brief Resource loading state
enum class ResourceState : uint8_t {
  Unloaded,   ///< Not loaded
  Loading,    ///< Currently loading in background
  Loaded,     ///< Successfully loaded and ready
  Failed,     ///< Loading failed
  Unloading   ///< Being unloaded
};

/// @brief Resource loading priority
enum class LoadPriority : uint8_t {
  Low = 0,      ///< Background loading, can wait
  Normal = 1,   ///< Standard priority
  High = 2,     ///< Load soon
  Immediate = 3 ///< Load as fast as possible (still async)
};

/// @brief Statistics for resource loading
struct ResourceStats {
  size_t totalResources{0};
  size_t loadedResources{0};
  size_t loadingResources{0};
  size_t failedResources{0};
  size_t totalMemoryBytes{0};
  size_t pendingLoads{0};
  double avgLoadTimeMs{0.0};
};

/// @brief Metadata for a resource
struct ResourceMetadata {
  std::string path;
  std::string name;
  std::type_index type{typeid(void)};
  size_t sizeBytes{0};
  std::chrono::system_clock::time_point lastModified;
  std::chrono::system_clock::time_point loadedAt;
  double loadTimeMs{0.0};
  uint32_t refCount{0};
  ResourceState state{ResourceState::Unloaded};
  std::string errorMessage;
};

/// @brief Type-safe handle to a resource with reference counting
/// 
/// ResourceHandle provides safe access to resources with automatic reference
/// counting. When all handles to a resource are destroyed, the resource
/// can be automatically unloaded if configured.
template <typename T>
class ResourceHandle {
public:
  ResourceHandle() = default;
  
  ResourceHandle(std::shared_ptr<T> resource, 
                 std::shared_ptr<ResourceMetadata> metadata,
                 ResourceManager* manager)
    : m_resource(std::move(resource))
    , m_metadata(std::move(metadata))
    , m_manager(manager) {
    if (m_metadata) {
      m_metadata->refCount++;
    }
  }
  
  ResourceHandle(const ResourceHandle& other)
    : m_resource(other.m_resource)
    , m_metadata(other.m_metadata)
    , m_manager(other.m_manager) {
    if (m_metadata) {
      m_metadata->refCount++;
    }
  }
  
  ResourceHandle(ResourceHandle&& other) noexcept
    : m_resource(std::move(other.m_resource))
    , m_metadata(std::move(other.m_metadata))
    , m_manager(other.m_manager) {
    other.m_manager = nullptr;
  }
  
  ResourceHandle& operator=(const ResourceHandle& other) {
    if (this != &other) {
      Release();
      m_resource = other.m_resource;
      m_metadata = other.m_metadata;
      m_manager = other.m_manager;
      if (m_metadata) {
        m_metadata->refCount++;
      }
    }
    return *this;
  }
  
  ResourceHandle& operator=(ResourceHandle&& other) noexcept {
    if (this != &other) {
      Release();
      m_resource = std::move(other.m_resource);
      m_metadata = std::move(other.m_metadata);
      m_manager = other.m_manager;
      other.m_manager = nullptr;
    }
    return *this;
  }
  
  ~ResourceHandle() {
    Release();
  }
  
  /// @brief Check if handle points to a valid, loaded resource
  [[nodiscard]] bool IsValid() const noexcept {
    return m_resource != nullptr && m_metadata && 
           m_metadata->state == ResourceState::Loaded;
  }
  
  /// @brief Check if the resource is currently loading
  [[nodiscard]] bool IsLoading() const noexcept {
    return m_metadata && m_metadata->state == ResourceState::Loading;
  }
  
  /// @brief Check if loading failed
  [[nodiscard]] bool HasFailed() const noexcept {
    return m_metadata && m_metadata->state == ResourceState::Failed;
  }
  
  /// @brief Get the current state
  [[nodiscard]] ResourceState GetState() const noexcept {
    return m_metadata ? m_metadata->state : ResourceState::Unloaded;
  }
  
  /// @brief Get error message if loading failed
  [[nodiscard]] std::string GetError() const {
    return m_metadata ? m_metadata->errorMessage : "";
  }
  
  /// @brief Get the resource path
  [[nodiscard]] std::string GetPath() const {
    return m_metadata ? m_metadata->path : "";
  }
  
  /// @brief Get resource metadata
  [[nodiscard]] const ResourceMetadata* GetMetadata() const {
    return m_metadata.get();
  }
  
  /// @brief Get pointer to the resource
  [[nodiscard]] T* Get() noexcept { return m_resource.get(); }
  [[nodiscard]] const T* Get() const noexcept { return m_resource.get(); }
  
  /// @brief Dereference operators
  T& operator*() { return *m_resource; }
  const T& operator*() const { return *m_resource; }
  T* operator->() noexcept { return m_resource.get(); }
  const T* operator->() const noexcept { return m_resource.get(); }
  
  /// @brief Boolean conversion
  explicit operator bool() const noexcept { return IsValid(); }
  
private:
  void Release() {
    if (m_metadata) {
      m_metadata->refCount--;
    }
    m_resource.reset();
    m_metadata.reset();
  }
  
  std::shared_ptr<T> m_resource;
  std::shared_ptr<ResourceMetadata> m_metadata;
  ResourceManager* m_manager{nullptr};
};

/// @brief Base interface for resource loaders
class IResourceLoader {
public:
  virtual ~IResourceLoader() = default;
  
  /// @brief Get the resource type this loader handles
  [[nodiscard]] virtual std::type_index GetResourceType() const = 0;
  
  /// @brief Get file extensions this loader supports
  [[nodiscard]] virtual std::vector<std::string> GetSupportedExtensions() const = 0;
  
  /// @brief Load a resource from file (called on worker thread)
  [[nodiscard]] virtual std::shared_ptr<void> Load(
    const std::filesystem::path& path,
    std::string& outError) = 0;
  
  /// @brief Unload/cleanup a resource
  virtual void Unload(std::shared_ptr<void> resource) = 0;
  
  /// @brief Estimate memory size of a loaded resource
  [[nodiscard]] virtual size_t EstimateSize(const std::shared_ptr<void>& resource) const {
    return 0;
  }
  
  /// @brief Check if the resource file has been modified
  [[nodiscard]] virtual bool HasChanged(
    const std::filesystem::path& path,
    std::chrono::system_clock::time_point lastLoaded) const {
    if (!std::filesystem::exists(path)) return false;
    auto lastWrite = std::filesystem::last_write_time(path);
    auto lastWriteSys = std::chrono::clock_cast<std::chrono::system_clock>(lastWrite);
    return lastWriteSys > lastLoaded;
  }
};

/// @brief Typed resource loader base class
template <typename T>
class ResourceLoader : public IResourceLoader {
public:
  [[nodiscard]] std::type_index GetResourceType() const override {
    return typeid(T);
  }
  
  [[nodiscard]] std::shared_ptr<void> Load(
    const std::filesystem::path& path,
    std::string& outError) override {
    return LoadTyped(path, outError);
  }
  
  void Unload(std::shared_ptr<void> resource) override {
    UnloadTyped(std::static_pointer_cast<T>(resource));
  }
  
  [[nodiscard]] size_t EstimateSize(const std::shared_ptr<void>& resource) const override {
    return EstimateSizeTyped(std::static_pointer_cast<T>(resource));
  }
  
protected:
  /// @brief Load resource (implement in derived class)
  [[nodiscard]] virtual std::shared_ptr<T> LoadTyped(
    const std::filesystem::path& path,
    std::string& outError) = 0;
  
  /// @brief Unload resource
  virtual void UnloadTyped(std::shared_ptr<T> resource) {
    // Default: let shared_ptr handle destruction
  }
  
  /// @brief Estimate memory size
  [[nodiscard]] virtual size_t EstimateSizeTyped(const std::shared_ptr<T>& resource) const {
    return sizeof(T);
  }
};

/// @brief Load request for the work queue
struct LoadRequest {
  std::string path;
  std::type_index type{typeid(void)};
  LoadPriority priority{LoadPriority::Normal};
  std::function<void(bool success)> callback;
  
  bool operator<(const LoadRequest& other) const {
    return static_cast<int>(priority) < static_cast<int>(other.priority);
  }
};

/// @brief Central resource manager with async loading and caching
///
/// @code
/// ResourceManager manager;
/// manager.Initialize(4); // 4 worker threads
///
/// // Register loaders
/// manager.RegisterLoader<Texture>(std::make_unique<TextureLoader>());
/// manager.RegisterLoader<Mesh>(std::make_unique<MeshLoader>());
///
/// // Async load
/// auto textureHandle = manager.LoadAsync<Texture>("textures/diffuse.png");
///
/// // Check if ready
/// if (textureHandle.IsLoading()) {
///   ShowLoadingSpinner();
/// } else if (textureHandle.IsValid()) {
///   UseTexture(textureHandle.Get());
/// }
///
/// // Sync load (blocks)
/// auto meshHandle = manager.Load<Mesh>("meshes/player.obj");
///
/// // Hot reload check
/// manager.CheckForChanges();
/// @endcode
class ResourceManager {
public:
  using ChangeCallback = std::function<void(const std::string& path)>;
  
  ResourceManager() = default;
  ~ResourceManager() { Shutdown(); }
  
  ResourceManager(const ResourceManager&) = delete;
  ResourceManager& operator=(const ResourceManager&) = delete;
  
  // ===========================================================================
  // Lifecycle
  // ===========================================================================
  
  /// @brief Initialize the resource manager with worker threads
  /// @param numWorkerThreads Number of background loading threads (0 = hardware concurrency)
  void Initialize(size_t numWorkerThreads = 0) {
    if (m_initialized) return;
    
    if (numWorkerThreads == 0) {
      numWorkerThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
    }
    
    m_running = true;
    m_workers.reserve(numWorkerThreads);
    
    for (size_t i = 0; i < numWorkerThreads; ++i) {
      m_workers.emplace_back([this] { WorkerThread(); });
    }
    
    m_initialized = true;
  }
  
  /// @brief Shutdown the resource manager and unload all resources
  void Shutdown() {
    if (!m_initialized) return;
    
    // Stop workers
    {
      std::lock_guard lock(m_queueMutex);
      m_running = false;
    }
    m_queueCondition.notify_all();
    
    for (auto& worker : m_workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
    m_workers.clear();
    
    // Unload all resources
    UnloadAll();
    
    m_initialized = false;
  }
  
  // ===========================================================================
  // Loader Registration
  // ===========================================================================
  
  /// @brief Register a resource loader for a type
  template <typename T>
  void RegisterLoader(std::unique_ptr<ResourceLoader<T>> loader) {
    std::lock_guard lock(m_loaderMutex);
    
    // Register by type
    m_loaders[typeid(T)] = std::move(loader);
    
    // Register extensions
    auto* loaderPtr = static_cast<IResourceLoader*>(m_loaders[typeid(T)].get());
    for (const auto& ext : loaderPtr->GetSupportedExtensions()) {
      m_extensionToType[ext] = typeid(T);
    }
  }
  
  /// @brief Check if a loader exists for a type
  template <typename T>
  [[nodiscard]] bool HasLoader() const {
    std::shared_lock lock(m_loaderMutex);
    return m_loaders.contains(typeid(T));
  }
  
  // ===========================================================================
  // Resource Loading
  // ===========================================================================
  
  /// @brief Load a resource synchronously (blocks until loaded)
  template <typename T>
  [[nodiscard]] ResourceHandle<T> Load(const std::string& path) {
    auto handle = LoadAsync<T>(path, LoadPriority::Immediate);
    
    // Wait for completion
    while (handle.IsLoading()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    return handle;
  }
  
  /// @brief Load a resource asynchronously
  template <typename T>
  [[nodiscard]] ResourceHandle<T> LoadAsync(
    const std::string& path,
    LoadPriority priority = LoadPriority::Normal,
    std::function<void(bool)> callback = nullptr) {
    
    const std::string normalizedPath = NormalizePath(path);
    const std::type_index typeIdx = typeid(T);
    const std::string cacheKey = MakeCacheKey(normalizedPath, typeIdx);
    
    // Check cache first
    {
      std::shared_lock lock(m_cacheMutex);
      auto it = m_resourceCache.find(cacheKey);
      if (it != m_resourceCache.end()) {
        auto& entry = it->second;
        if (entry.state == ResourceState::Loaded || 
            entry.state == ResourceState::Loading) {
          return ResourceHandle<T>(
            std::static_pointer_cast<T>(entry.resource),
            entry.metadata,
            this
          );
        }
      }
    }
    
    // Create cache entry
    auto metadata = std::make_shared<ResourceMetadata>();
    metadata->path = normalizedPath;
    metadata->name = std::filesystem::path(normalizedPath).filename().string();
    metadata->type = typeIdx;
    metadata->state = ResourceState::Loading;
    
    {
      std::unique_lock lock(m_cacheMutex);
      CacheEntry entry;
      entry.metadata = metadata;
      entry.state = ResourceState::Loading;
      m_resourceCache[cacheKey] = std::move(entry);
    }
    
    // Queue load request
    {
      std::lock_guard lock(m_queueMutex);
      LoadRequest request;
      request.path = normalizedPath;
      request.type = typeIdx;
      request.priority = priority;
      request.callback = std::move(callback);
      m_loadQueue.push(std::move(request));
    }
    m_queueCondition.notify_one();
    
    // Return handle (will be updated when loading completes)
    return ResourceHandle<T>(nullptr, metadata, this);
  }
  
  /// @brief Get an already-loaded resource (returns invalid handle if not loaded)
  template <typename T>
  [[nodiscard]] ResourceHandle<T> Get(const std::string& path) {
    const std::string normalizedPath = NormalizePath(path);
    const std::string cacheKey = MakeCacheKey(normalizedPath, typeid(T));
    
    std::shared_lock lock(m_cacheMutex);
    auto it = m_resourceCache.find(cacheKey);
    if (it != m_resourceCache.end() && it->second.state == ResourceState::Loaded) {
      return ResourceHandle<T>(
        std::static_pointer_cast<T>(it->second.resource),
        it->second.metadata,
        this
      );
    }
    return ResourceHandle<T>();
  }
  
  /// @brief Check if a resource is loaded
  [[nodiscard]] bool IsLoaded(const std::string& path) const {
    const std::string normalizedPath = NormalizePath(path);
    
    std::shared_lock lock(m_cacheMutex);
    for (const auto& [key, entry] : m_resourceCache) {
      if (entry.metadata && entry.metadata->path == normalizedPath &&
          entry.state == ResourceState::Loaded) {
        return true;
      }
    }
    return false;
  }
  
  // ===========================================================================
  // Resource Unloading
  // ===========================================================================
  
  /// @brief Unload a specific resource
  template <typename T>
  void Unload(const std::string& path) {
    const std::string normalizedPath = NormalizePath(path);
    const std::string cacheKey = MakeCacheKey(normalizedPath, typeid(T));
    
    std::unique_lock lock(m_cacheMutex);
    auto it = m_resourceCache.find(cacheKey);
    if (it != m_resourceCache.end()) {
      UnloadEntry(it->second);
      m_resourceCache.erase(it);
    }
  }
  
  /// @brief Unload all resources
  void UnloadAll() {
    std::unique_lock lock(m_cacheMutex);
    for (auto& [key, entry] : m_resourceCache) {
      UnloadEntry(entry);
    }
    m_resourceCache.clear();
  }
  
  /// @brief Unload resources with zero references
  void UnloadUnused() {
    std::unique_lock lock(m_cacheMutex);
    
    std::vector<std::string> toRemove;
    for (auto& [key, entry] : m_resourceCache) {
      if (entry.metadata && entry.metadata->refCount == 0) {
        UnloadEntry(entry);
        toRemove.push_back(key);
      }
    }
    
    for (const auto& key : toRemove) {
      m_resourceCache.erase(key);
    }
  }
  
  // ===========================================================================
  // Hot Reloading
  // ===========================================================================
  
  /// @brief Enable or disable hot reloading
  void SetHotReloadEnabled(bool enabled) { m_hotReloadEnabled = enabled; }
  [[nodiscard]] bool IsHotReloadEnabled() const { return m_hotReloadEnabled; }
  
  /// @brief Check for changed files and reload them
  /// @return Number of resources reloaded
  size_t CheckForChanges() {
    if (!m_hotReloadEnabled) return 0;
    
    std::vector<std::string> toReload;
    
    {
      std::shared_lock lock(m_cacheMutex);
      for (const auto& [key, entry] : m_resourceCache) {
        if (entry.state != ResourceState::Loaded || !entry.metadata) continue;
        
        auto loader = GetLoader(entry.metadata->type);
        if (loader && loader->HasChanged(entry.metadata->path, entry.metadata->loadedAt)) {
          toReload.push_back(key);
        }
      }
    }
    
    for (const auto& key : toReload) {
      ReloadResource(key);
    }
    
    // Notify callbacks
    for (const auto& key : toReload) {
      std::shared_lock lock(m_cacheMutex);
      auto it = m_resourceCache.find(key);
      if (it != m_resourceCache.end() && it->second.metadata) {
        for (const auto& callback : m_changeCallbacks) {
          callback(it->second.metadata->path);
        }
      }
    }
    
    return toReload.size();
  }
  
  /// @brief Register a callback for when resources change
  void OnResourceChanged(ChangeCallback callback) {
    m_changeCallbacks.push_back(std::move(callback));
  }
  
  // ===========================================================================
  // Statistics
  // ===========================================================================
  
  /// @brief Get resource statistics
  [[nodiscard]] ResourceStats GetStats() const {
    ResourceStats stats;
    
    std::shared_lock lock(m_cacheMutex);
    for (const auto& [key, entry] : m_resourceCache) {
      stats.totalResources++;
      
      switch (entry.state) {
        case ResourceState::Loaded:
          stats.loadedResources++;
          if (entry.metadata) {
            stats.totalMemoryBytes += entry.metadata->sizeBytes;
          }
          break;
        case ResourceState::Loading:
          stats.loadingResources++;
          break;
        case ResourceState::Failed:
          stats.failedResources++;
          break;
        default:
          break;
      }
    }
    
    {
      std::lock_guard qLock(m_queueMutex);
      stats.pendingLoads = m_loadQueue.size();
    }
    
    if (m_totalLoads > 0) {
      stats.avgLoadTimeMs = m_totalLoadTimeMs / static_cast<double>(m_totalLoads);
    }
    
    return stats;
  }
  
  /// @brief Get all loaded resource paths
  [[nodiscard]] std::vector<std::string> GetLoadedPaths() const {
    std::vector<std::string> paths;
    
    std::shared_lock lock(m_cacheMutex);
    for (const auto& [key, entry] : m_resourceCache) {
      if (entry.state == ResourceState::Loaded && entry.metadata) {
        paths.push_back(entry.metadata->path);
      }
    }
    
    return paths;
  }
  
  // ===========================================================================
  // Path Management
  // ===========================================================================
  
  /// @brief Set the base path for resource loading
  void SetBasePath(const std::filesystem::path& path) {
    m_basePath = path;
  }
  
  /// @brief Add a search path for resources
  void AddSearchPath(const std::filesystem::path& path) {
    m_searchPaths.push_back(path);
  }
  
  /// @brief Resolve a resource path to an absolute path
  [[nodiscard]] std::filesystem::path ResolvePath(const std::string& path) const {
    std::filesystem::path fsPath(path);
    
    // Check if already absolute and exists
    if (fsPath.is_absolute() && std::filesystem::exists(fsPath)) {
      return fsPath;
    }
    
    // Check base path
    if (!m_basePath.empty()) {
      auto fullPath = m_basePath / fsPath;
      if (std::filesystem::exists(fullPath)) {
        return fullPath;
      }
    }
    
    // Check search paths
    for (const auto& searchPath : m_searchPaths) {
      auto fullPath = searchPath / fsPath;
      if (std::filesystem::exists(fullPath)) {
        return fullPath;
      }
    }
    
    // Return as-is
    return fsPath;
  }

private:
  struct CacheEntry {
    std::shared_ptr<void> resource;
    std::shared_ptr<ResourceMetadata> metadata;
    ResourceState state{ResourceState::Unloaded};
  };
  
  void WorkerThread() {
    while (true) {
      LoadRequest request;
      
      {
        std::unique_lock lock(m_queueMutex);
        m_queueCondition.wait(lock, [this] {
          return !m_running || !m_loadQueue.empty();
        });
        
        if (!m_running && m_loadQueue.empty()) {
          return;
        }
        
        request = std::move(const_cast<LoadRequest&>(m_loadQueue.top()));
        m_loadQueue.pop();
      }
      
      ProcessLoadRequest(request);
    }
  }
  
  void ProcessLoadRequest(LoadRequest& request) {
    const std::string cacheKey = MakeCacheKey(request.path, request.type);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Get loader
    auto loader = GetLoader(request.type);
    if (!loader) {
      MarkFailed(cacheKey, "No loader registered for resource type");
      if (request.callback) request.callback(false);
      return;
    }
    
    // Resolve path
    auto fullPath = ResolvePath(request.path);
    if (!std::filesystem::exists(fullPath)) {
      MarkFailed(cacheKey, "File not found: " + fullPath.string());
      if (request.callback) request.callback(false);
      return;
    }
    
    // Load resource
    std::string error;
    auto resource = loader->Load(fullPath, error);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    double loadTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    if (!resource) {
      MarkFailed(cacheKey, error.empty() ? "Unknown loading error" : error);
      if (request.callback) request.callback(false);
      return;
    }
    
    // Update cache
    {
      std::unique_lock lock(m_cacheMutex);
      auto it = m_resourceCache.find(cacheKey);
      if (it != m_resourceCache.end()) {
        it->second.resource = resource;
        it->second.state = ResourceState::Loaded;
        if (it->second.metadata) {
          it->second.metadata->state = ResourceState::Loaded;
          it->second.metadata->sizeBytes = loader->EstimateSize(resource);
          it->second.metadata->loadedAt = std::chrono::system_clock::now();
          it->second.metadata->loadTimeMs = loadTimeMs;
        }
      }
    }
    
    // Update stats
    m_totalLoads++;
    m_totalLoadTimeMs += loadTimeMs;
    
    if (request.callback) request.callback(true);
  }
  
  void MarkFailed(const std::string& cacheKey, const std::string& error) {
    std::unique_lock lock(m_cacheMutex);
    auto it = m_resourceCache.find(cacheKey);
    if (it != m_resourceCache.end()) {
      it->second.state = ResourceState::Failed;
      if (it->second.metadata) {
        it->second.metadata->state = ResourceState::Failed;
        it->second.metadata->errorMessage = error;
      }
    }
  }
  
  void ReloadResource(const std::string& cacheKey) {
    std::shared_ptr<ResourceMetadata> metadata;
    
    {
      std::shared_lock lock(m_cacheMutex);
      auto it = m_resourceCache.find(cacheKey);
      if (it == m_resourceCache.end()) return;
      metadata = it->second.metadata;
    }
    
    if (!metadata) return;
    
    // Queue reload
    {
      std::lock_guard lock(m_queueMutex);
      LoadRequest request;
      request.path = metadata->path;
      request.type = metadata->type;
      request.priority = LoadPriority::High;
      m_loadQueue.push(std::move(request));
    }
    m_queueCondition.notify_one();
  }
  
  void UnloadEntry(CacheEntry& entry) {
    if (entry.resource && entry.metadata) {
      auto loader = GetLoader(entry.metadata->type);
      if (loader) {
        loader->Unload(entry.resource);
      }
    }
    entry.resource.reset();
    entry.state = ResourceState::Unloaded;
    if (entry.metadata) {
      entry.metadata->state = ResourceState::Unloaded;
    }
  }
  
  [[nodiscard]] IResourceLoader* GetLoader(std::type_index type) const {
    std::shared_lock lock(m_loaderMutex);
    auto it = m_loaders.find(type);
    return it != m_loaders.end() ? it->second.get() : nullptr;
  }
  
  [[nodiscard]] static std::string NormalizePath(const std::string& path) {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
  }
  
  [[nodiscard]] static std::string MakeCacheKey(const std::string& path, std::type_index type) {
    return path + "@" + std::string(type.name());
  }
  
  // Configuration
  std::filesystem::path m_basePath;
  std::vector<std::filesystem::path> m_searchPaths;
  bool m_hotReloadEnabled{false};
  
  // Threading
  std::vector<std::thread> m_workers;
  std::atomic<bool> m_running{false};
  std::atomic<bool> m_initialized{false};
  
  // Load queue
  mutable std::mutex m_queueMutex;
  std::condition_variable m_queueCondition;
  std::priority_queue<LoadRequest> m_loadQueue;
  
  // Loaders
  mutable std::shared_mutex m_loaderMutex;
  std::unordered_map<std::type_index, std::unique_ptr<IResourceLoader>> m_loaders;
  std::unordered_map<std::string, std::type_index> m_extensionToType;
  
  // Cache
  mutable std::shared_mutex m_cacheMutex;
  std::unordered_map<std::string, CacheEntry> m_resourceCache;
  
  // Callbacks
  std::vector<ChangeCallback> m_changeCallbacks;
  
  // Stats
  std::atomic<size_t> m_totalLoads{0};
  std::atomic<double> m_totalLoadTimeMs{0.0};
};

} // namespace Aetherion::Assets
