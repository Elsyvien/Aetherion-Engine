#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

namespace Aetherion::Core {

/// @brief Handle to a pooled object with generational index
template <typename T> struct PoolHandle {
  uint32_t index{UINT32_MAX};
  uint32_t generation{0};

  [[nodiscard]] bool IsValid() const noexcept { return index != UINT32_MAX; }

  bool operator==(const PoolHandle &other) const noexcept {
    return index == other.index && generation == other.generation;
  }

  bool operator!=(const PoolHandle &other) const noexcept {
    return !(*this == other);
  }

  static PoolHandle Invalid() { return PoolHandle{}; }
};

/// @brief Generic object pool with generational handles for safe access
///
/// Provides O(1) allocation and deallocation with automatic memory reuse.
/// Generational indices prevent use-after-free bugs.
///
/// @code
/// ObjectPool<Entity> pool(1024);
///
/// auto handle = pool.Acquire();
/// Entity& entity = pool.Get(handle);
/// entity.name = "Player";
///
/// pool.Release(handle);
/// // handle is now invalid, Get() will fail safely
/// @endcode
template <typename T> class ObjectPool {
public:
  using Handle = PoolHandle<T>;

  /// @brief Create a pool with initial capacity
  explicit ObjectPool(size_t initialCapacity = 64) { Reserve(initialCapacity); }

  /// @brief Reserve capacity for objects
  void Reserve(size_t capacity) {
    if (capacity <= m_objects.size())
      return;

    const size_t oldSize = m_objects.size();
    m_objects.resize(capacity);
    m_generations.resize(capacity, 0);
    m_active.resize(capacity, false);

    // Add new slots to free list
    for (size_t i = oldSize; i < capacity; ++i) {
      m_freeList.push_back(static_cast<uint32_t>(i));
    }
  }

  /// @brief Acquire an object from the pool
  /// @return Handle to the acquired object
  [[nodiscard]] Handle Acquire() {
    std::lock_guard lock(m_mutex);

    if (m_freeList.empty()) {
      // Grow the pool
      Reserve(m_objects.size() * 2);
    }

    const uint32_t index = m_freeList.back();
    m_freeList.pop_back();

    m_active[index] = true;
    m_activeCount++;

    // Construct object in place
    new (&m_objects[index]) T();

    return Handle{index, m_generations[index]};
  }

  /// @brief Acquire an object with constructor arguments
  template <typename... Args> [[nodiscard]] Handle Acquire(Args &&...args) {
    std::lock_guard lock(m_mutex);

    if (m_freeList.empty()) {
      Reserve(m_objects.size() * 2);
    }

    const uint32_t index = m_freeList.back();
    m_freeList.pop_back();

    m_active[index] = true;
    m_activeCount++;

    // Construct object in place with arguments
    new (&m_objects[index]) T(std::forward<Args>(args)...);

    return Handle{index, m_generations[index]};
  }

  /// @brief Release an object back to the pool
  void Release(Handle handle) {
    std::lock_guard lock(m_mutex);

    if (!IsValidLocked(handle))
      return;

    const uint32_t index = handle.index;

    // Destruct object
    m_objects[index].~T();

    m_active[index] = false;
    m_generations[index]++; // Increment generation to invalidate old handles
    m_activeCount--;

    m_freeList.push_back(index);
  }

  /// @brief Check if a handle is valid
  [[nodiscard]] bool IsValid(Handle handle) const {
    std::lock_guard lock(m_mutex);
    return IsValidLocked(handle);
  }

  /// @brief Get a reference to an object
  /// @throws std::out_of_range if handle is invalid
  [[nodiscard]] T &Get(Handle handle) {
    std::lock_guard lock(m_mutex);
    if (!IsValidLocked(handle)) {
      throw std::out_of_range("Invalid pool handle");
    }
    return m_objects[handle.index];
  }

  /// @brief Get a const reference to an object
  [[nodiscard]] const T &Get(Handle handle) const {
    std::lock_guard lock(m_mutex);
    if (!IsValidLocked(handle)) {
      throw std::out_of_range("Invalid pool handle");
    }
    return m_objects[handle.index];
  }

  /// @brief Try to get a pointer to an object (returns nullptr if invalid)
  [[nodiscard]] T *TryGet(Handle handle) {
    std::lock_guard lock(m_mutex);
    if (!IsValidLocked(handle))
      return nullptr;
    return &m_objects[handle.index];
  }

  /// @brief Try to get a const pointer to an object
  [[nodiscard]] const T *TryGet(Handle handle) const {
    std::lock_guard lock(m_mutex);
    if (!IsValidLocked(handle))
      return nullptr;
    return &m_objects[handle.index];
  }

  /// @brief Get number of active objects
  [[nodiscard]] size_t ActiveCount() const noexcept {
    std::lock_guard lock(m_mutex);
    return m_activeCount;
  }

  /// @brief Get total capacity
  [[nodiscard]] size_t Capacity() const noexcept {
    std::lock_guard lock(m_mutex);
    return m_objects.size();
  }

  /// @brief Get number of free slots
  [[nodiscard]] size_t FreeCount() const noexcept {
    std::lock_guard lock(m_mutex);
    return m_freeList.size();
  }

  /// @brief Iterate over all active objects
  template <typename Func> void ForEach(Func &&func) {
    std::lock_guard lock(m_mutex);
    for (size_t i = 0; i < m_objects.size(); ++i) {
      if (m_active[i]) {
        Handle handle{static_cast<uint32_t>(i), m_generations[i]};
        func(handle, m_objects[i]);
      }
    }
  }

  /// @brief Iterate over all active objects (const)
  template <typename Func> void ForEach(Func &&func) const {
    std::lock_guard lock(m_mutex);
    for (size_t i = 0; i < m_objects.size(); ++i) {
      if (m_active[i]) {
        Handle handle{static_cast<uint32_t>(i), m_generations[i]};
        func(handle, m_objects[i]);
      }
    }
  }

  /// @brief Clear all objects and reset the pool
  void Clear() {
    std::lock_guard lock(m_mutex);

    // Destruct active objects
    for (size_t i = 0; i < m_objects.size(); ++i) {
      if (m_active[i]) {
        m_objects[i].~T();
      }
    }

    m_freeList.clear();
    for (size_t i = 0; i < m_objects.size(); ++i) {
      m_freeList.push_back(static_cast<uint32_t>(i));
      m_generations[i]++;
      m_active[i] = false;
    }
    m_activeCount = 0;
  }

private:
  [[nodiscard]] bool IsValidLocked(Handle handle) const noexcept {
    return handle.index < m_objects.size() && m_active[handle.index] &&
           m_generations[handle.index] == handle.generation;
  }

  mutable std::mutex m_mutex;
  std::vector<T> m_objects;
  std::vector<uint32_t> m_generations;
  std::vector<bool> m_active;
  std::vector<uint32_t> m_freeList;
  size_t m_activeCount{0};
};

/// @brief Fixed-size object pool without dynamic allocation
///
/// Useful for performance-critical paths where heap allocation is undesirable.
template <typename T, size_t Capacity> class FixedObjectPool {
public:
  using Handle = PoolHandle<T>;

  FixedObjectPool() {
    for (size_t i = 0; i < Capacity; ++i) {
      m_freeList[m_freeCount++] = static_cast<uint32_t>(i);
    }
  }

  ~FixedObjectPool() {
    // Destruct active objects
    for (size_t i = 0; i < Capacity; ++i) {
      if (m_active[i]) {
        GetStorage(i)->~T();
      }
    }
  }

  [[nodiscard]] Handle Acquire() {
    if (m_freeCount == 0) {
      return Handle::Invalid();
    }

    const uint32_t index = m_freeList[--m_freeCount];
    m_active[index] = true;
    m_activeCount++;

    new (GetStorage(index)) T();

    return Handle{index, m_generations[index]};
  }

  template <typename... Args> [[nodiscard]] Handle Acquire(Args &&...args) {
    if (m_freeCount == 0) {
      return Handle::Invalid();
    }

    const uint32_t index = m_freeList[--m_freeCount];
    m_active[index] = true;
    m_activeCount++;

    new (GetStorage(index)) T(std::forward<Args>(args)...);

    return Handle{index, m_generations[index]};
  }

  void Release(Handle handle) {
    if (!IsValid(handle))
      return;

    const uint32_t index = handle.index;

    GetStorage(index)->~T();

    m_active[index] = false;
    m_generations[index]++;
    m_activeCount--;

    m_freeList[m_freeCount++] = index;
  }

  [[nodiscard]] bool IsValid(Handle handle) const noexcept {
    return handle.index < Capacity && m_active[handle.index] &&
           m_generations[handle.index] == handle.generation;
  }

  [[nodiscard]] T &Get(Handle handle) {
    assert(IsValid(handle));
    return *GetStorage(handle.index);
  }

  [[nodiscard]] const T &Get(Handle handle) const {
    assert(IsValid(handle));
    return *GetStorage(handle.index);
  }

  [[nodiscard]] T *TryGet(Handle handle) {
    return IsValid(handle) ? GetStorage(handle.index) : nullptr;
  }

  [[nodiscard]] size_t ActiveCount() const noexcept { return m_activeCount; }
  [[nodiscard]] constexpr size_t GetCapacity() const noexcept {
    return Capacity;
  }
  [[nodiscard]] size_t FreeCount() const noexcept { return m_freeCount; }
  [[nodiscard]] bool IsFull() const noexcept { return m_freeCount == 0; }

private:
  T *GetStorage(size_t index) {
    return reinterpret_cast<T *>(&m_storage[index * sizeof(T)]);
  }

  const T *GetStorage(size_t index) const {
    return reinterpret_cast<const T *>(&m_storage[index * sizeof(T)]);
  }

  alignas(T) uint8_t m_storage[Capacity * sizeof(T)];
  std::array<uint32_t, Capacity> m_generations{};
  std::array<bool, Capacity> m_active{};
  std::array<uint32_t, Capacity> m_freeList{};
  size_t m_freeCount{0};
  size_t m_activeCount{0};
};

/// @brief RAII guard for automatically releasing pooled objects
template <typename T> class PoolGuard {
public:
  using Handle = PoolHandle<T>;

  PoolGuard(ObjectPool<T> &pool, Handle handle)
      : m_pool(&pool), m_handle(handle) {}

  ~PoolGuard() {
    if (m_handle.IsValid()) {
      m_pool->Release(m_handle);
    }
  }

  PoolGuard(const PoolGuard &) = delete;
  PoolGuard &operator=(const PoolGuard &) = delete;

  PoolGuard(PoolGuard &&other) noexcept
      : m_pool(other.m_pool), m_handle(other.m_handle) {
    other.m_handle = Handle::Invalid();
  }

  PoolGuard &operator=(PoolGuard &&other) noexcept {
    if (this != &other) {
      if (m_handle.IsValid()) {
        m_pool->Release(m_handle);
      }
      m_pool = other.m_pool;
      m_handle = other.m_handle;
      other.m_handle = Handle::Invalid();
    }
    return *this;
  }

  [[nodiscard]] Handle GetHandle() const noexcept { return m_handle; }
  [[nodiscard]] T &Get() { return m_pool->Get(m_handle); }
  [[nodiscard]] const T &Get() const { return m_pool->Get(m_handle); }

  /// @brief Release ownership without returning to pool
  Handle Release() noexcept {
    Handle h = m_handle;
    m_handle = Handle::Invalid();
    return h;
  }

private:
  ObjectPool<T> *m_pool;
  Handle m_handle;
};

} // namespace Aetherion::Core
