#pragma once

#include <algorithm>
#include <any>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Aetherion::Core {

/// @brief Unique identifier for event subscriptions
using EventSubscriptionId = uint64_t;

/// @brief Base class for all events (optional, for reflection/debugging)
struct EventBase {
  virtual ~EventBase() = default;
  [[nodiscard]] virtual const char *GetTypeName() const noexcept = 0;
};

/// @brief CRTP helper for event types
template <typename Derived> struct Event : EventBase {
  [[nodiscard]] const char *GetTypeName() const noexcept override {
    return typeid(Derived).name();
  }
};

/// @brief Type-safe event bus for decoupled communication between systems
///
/// The EventBus provides:
/// - Type-safe event subscription and publishing
/// - Automatic subscriber cleanup via RAII guards
/// - Thread-safe operations
/// - Priority-based handler ordering
/// - One-shot subscriptions
/// - Event queuing for deferred processing
///
/// Usage:
/// @code
/// struct PlayerDiedEvent : Event<PlayerDiedEvent> {
///   EntityId playerId;
///   float x, y, z;
/// };
///
/// EventBus bus;
/// auto sub = bus.Subscribe<PlayerDiedEvent>([](const PlayerDiedEvent& e) {
///   std::cout << "Player " << e.playerId << " died!\n";
/// });
///
/// bus.Publish(PlayerDiedEvent{playerId, x, y, z});
/// @endcode
class EventBus {
public:
  /// @brief Handler priority (lower = earlier execution)
  using Priority = int32_t;
  static constexpr Priority kDefaultPriority = 0;
  static constexpr Priority kHighPriority = -100;
  static constexpr Priority kLowPriority = 100;

  EventBus() = default;
  ~EventBus() = default;

  EventBus(const EventBus &) = delete;
  EventBus &operator=(const EventBus &) = delete;

  /// @brief RAII guard that automatically unsubscribes when destroyed
  class SubscriptionGuard {
  public:
    SubscriptionGuard() = default;
    SubscriptionGuard(EventBus *bus, std::type_index type,
                      EventSubscriptionId id)
        : m_bus(bus), m_type(type), m_id(id) {}
    ~SubscriptionGuard() { Unsubscribe(); }

    SubscriptionGuard(SubscriptionGuard &&other) noexcept
        : m_bus(other.m_bus), m_type(other.m_type), m_id(other.m_id) {
      other.m_bus = nullptr;
      other.m_id = 0;
    }

    SubscriptionGuard &operator=(SubscriptionGuard &&other) noexcept {
      if (this != &other) {
        Unsubscribe();
        m_bus = other.m_bus;
        m_type = other.m_type;
        m_id = other.m_id;
        other.m_bus = nullptr;
        other.m_id = 0;
      }
      return *this;
    }

    SubscriptionGuard(const SubscriptionGuard &) = delete;
    SubscriptionGuard &operator=(const SubscriptionGuard &) = delete;

    void Unsubscribe() {
      if (m_bus && m_id != 0) {
        m_bus->UnsubscribeById(m_type, m_id);
        m_bus = nullptr;
        m_id = 0;
      }
    }

    [[nodiscard]] bool IsValid() const noexcept {
      return m_bus != nullptr && m_id != 0;
    }
    [[nodiscard]] EventSubscriptionId GetId() const noexcept { return m_id; }

  private:
    EventBus *m_bus{nullptr};
    std::type_index m_type{typeid(void)};
    EventSubscriptionId m_id{0};
  };

  /// @brief Subscribe to an event type
  /// @tparam TEvent Event type to subscribe to
  /// @param handler Callback function
  /// @param priority Handler priority (lower = earlier)
  /// @return RAII guard that unsubscribes when destroyed
  template <typename TEvent>
  [[nodiscard]] SubscriptionGuard
  Subscribe(std::function<void(const TEvent &)> handler,
            Priority priority = kDefaultPriority) {
    std::lock_guard lock(m_mutex);

    auto typeIdx = std::type_index(typeid(TEvent));
    auto &handlers = m_handlers[typeIdx];

    EventSubscriptionId id = ++m_nextId;

    HandlerEntry entry;
    entry.id = id;
    entry.priority = priority;
    entry.handler = [h = std::move(handler)](const std::any &event) {
      h(std::any_cast<const TEvent &>(event));
    };
    entry.oneShot = false;

    handlers.push_back(std::move(entry));
    SortHandlers(handlers);

    return SubscriptionGuard(this, typeIdx, id);
  }

  /// @brief Subscribe for a single event only (auto-unsubscribes after first
  /// trigger)
  template <typename TEvent>
  [[nodiscard]] SubscriptionGuard
  SubscribeOnce(std::function<void(const TEvent &)> handler,
                Priority priority = kDefaultPriority) {
    std::lock_guard lock(m_mutex);

    auto typeIdx = std::type_index(typeid(TEvent));
    auto &handlers = m_handlers[typeIdx];

    EventSubscriptionId id = ++m_nextId;

    HandlerEntry entry;
    entry.id = id;
    entry.priority = priority;
    entry.handler = [h = std::move(handler)](const std::any &event) {
      h(std::any_cast<const TEvent &>(event));
    };
    entry.oneShot = true;

    handlers.push_back(std::move(entry));
    SortHandlers(handlers);

    return SubscriptionGuard(this, typeIdx, id);
  }

  /// @brief Publish an event immediately to all subscribers
  /// @tparam TEvent Event type
  /// @param event Event data
  template <typename TEvent> void Publish(const TEvent &event) {
    std::vector<HandlerEntry> toCall;
    std::vector<EventSubscriptionId> toRemove;

    {
      std::lock_guard lock(m_mutex);

      auto typeIdx = std::type_index(typeid(TEvent));
      auto it = m_handlers.find(typeIdx);
      if (it == m_handlers.end()) {
        return;
      }

      // Copy handlers to call outside lock
      toCall = it->second;
    }

    // Call handlers outside lock to prevent deadlocks
    std::any eventAny = event;
    for (const auto &entry : toCall) {
      entry.handler(eventAny);
      if (entry.oneShot) {
        toRemove.push_back(entry.id);
      }
    }

    // Remove one-shot handlers
    if (!toRemove.empty()) {
      std::lock_guard lock(m_mutex);
      auto typeIdx = std::type_index(typeid(TEvent));
      auto it = m_handlers.find(typeIdx);
      if (it != m_handlers.end()) {
        auto &handlers = it->second;
        handlers.erase(
            std::remove_if(handlers.begin(), handlers.end(),
                           [&toRemove](const HandlerEntry &e) {
                             return std::find(toRemove.begin(), toRemove.end(),
                                              e.id) != toRemove.end();
                           }),
            handlers.end());
      }
    }
  }

  /// @brief Queue an event for deferred processing
  template <typename TEvent> void Queue(const TEvent &event) {
    std::lock_guard lock(m_queueMutex);
    m_eventQueue.push_back(
        {std::type_index(typeid(TEvent)), std::any(event)});
  }

  /// @brief Process all queued events
  void ProcessQueue() {
    std::vector<QueuedEvent> toProcess;

    {
      std::lock_guard lock(m_queueMutex);
      std::swap(toProcess, m_eventQueue);
    }

    for (const auto &qe : toProcess) {
      PublishAny(qe.type, qe.event);
    }
  }

  /// @brief Clear all queued events without processing
  void ClearQueue() {
    std::lock_guard lock(m_queueMutex);
    m_eventQueue.clear();
  }

  /// @brief Get number of subscribers for an event type
  template <typename TEvent>
  [[nodiscard]] size_t GetSubscriberCount() const {
    std::lock_guard lock(m_mutex);
    auto typeIdx = std::type_index(typeid(TEvent));
    auto it = m_handlers.find(typeIdx);
    return it != m_handlers.end() ? it->second.size() : 0;
  }

  /// @brief Check if there are any subscribers for an event type
  template <typename TEvent> [[nodiscard]] bool HasSubscribers() const {
    return GetSubscriberCount<TEvent>() > 0;
  }

  /// @brief Remove all subscribers for a specific event type
  template <typename TEvent> void ClearSubscribers() {
    std::lock_guard lock(m_mutex);
    auto typeIdx = std::type_index(typeid(TEvent));
    m_handlers.erase(typeIdx);
  }

  /// @brief Remove all subscribers for all event types
  void ClearAllSubscribers() {
    std::lock_guard lock(m_mutex);
    m_handlers.clear();
  }

private:
  struct HandlerEntry {
    EventSubscriptionId id{0};
    Priority priority{0};
    std::function<void(const std::any &)> handler;
    bool oneShot{false};
  };

  struct QueuedEvent {
    std::type_index type;
    std::any event;
  };

  void UnsubscribeById(std::type_index type, EventSubscriptionId id) {
    std::lock_guard lock(m_mutex);
    auto it = m_handlers.find(type);
    if (it != m_handlers.end()) {
      auto &handlers = it->second;
      handlers.erase(std::remove_if(handlers.begin(), handlers.end(),
                                    [id](const HandlerEntry &e) {
                                      return e.id == id;
                                    }),
                     handlers.end());
    }
  }

  void SortHandlers(std::vector<HandlerEntry> &handlers) {
    std::stable_sort(handlers.begin(), handlers.end(),
                     [](const HandlerEntry &a, const HandlerEntry &b) {
                       return a.priority < b.priority;
                     });
  }

  void PublishAny(std::type_index type, const std::any &event) {
    std::vector<HandlerEntry> toCall;
    std::vector<EventSubscriptionId> toRemove;

    {
      std::lock_guard lock(m_mutex);
      auto it = m_handlers.find(type);
      if (it == m_handlers.end()) {
        return;
      }
      toCall = it->second;
    }

    for (const auto &entry : toCall) {
      entry.handler(event);
      if (entry.oneShot) {
        toRemove.push_back(entry.id);
      }
    }

    if (!toRemove.empty()) {
      std::lock_guard lock(m_mutex);
      auto it = m_handlers.find(type);
      if (it != m_handlers.end()) {
        auto &handlers = it->second;
        handlers.erase(
            std::remove_if(handlers.begin(), handlers.end(),
                           [&toRemove](const HandlerEntry &e) {
                             return std::find(toRemove.begin(), toRemove.end(),
                                              e.id) != toRemove.end();
                           }),
            handlers.end());
      }
    }
  }

  mutable std::mutex m_mutex;
  std::mutex m_queueMutex;
  std::unordered_map<std::type_index, std::vector<HandlerEntry>> m_handlers;
  std::vector<QueuedEvent> m_eventQueue;
  EventSubscriptionId m_nextId{0};
};

// ============================================================================
// Common Engine Events
// ============================================================================

/// @brief Fired when an entity is created
struct EntityCreatedEvent : Event<EntityCreatedEvent> {
  uint64_t entityId{0};
  std::string name;
};

/// @brief Fired when an entity is destroyed
struct EntityDestroyedEvent : Event<EntityDestroyedEvent> {
  uint64_t entityId{0};
};

/// @brief Fired when an entity is selected in the editor
struct EntitySelectedEvent : Event<EntitySelectedEvent> {
  uint64_t entityId{0};
  uint64_t previousEntityId{0};
};

/// @brief Fired when a component is added to an entity
struct ComponentAddedEvent : Event<ComponentAddedEvent> {
  uint64_t entityId{0};
  std::string componentType;
};

/// @brief Fired when a component is removed from an entity
struct ComponentRemovedEvent : Event<ComponentRemovedEvent> {
  uint64_t entityId{0};
  std::string componentType;
};

/// @brief Fired when a scene is loaded
struct SceneLoadedEvent : Event<SceneLoadedEvent> {
  std::string scenePath;
  std::string sceneName;
};

/// @brief Fired when a scene is unloaded
struct SceneUnloadedEvent : Event<SceneUnloadedEvent> {
  std::string sceneName;
};

/// @brief Fired when simulation starts
struct SimulationStartedEvent : Event<SimulationStartedEvent> {};

/// @brief Fired when simulation stops
struct SimulationStoppedEvent : Event<SimulationStoppedEvent> {};

/// @brief Fired when simulation is paused
struct SimulationPausedEvent : Event<SimulationPausedEvent> {
  bool paused{true};
};

/// @brief Fired when an asset is loaded
struct AssetLoadedEvent : Event<AssetLoadedEvent> {
  std::string assetId;
  std::string assetType;
};

/// @brief Fired when an asset is modified (hot reload)
struct AssetModifiedEvent : Event<AssetModifiedEvent> {
  std::string assetId;
  std::string assetType;
};

/// @brief Fired on physics collision enter
struct CollisionEnterEvent : Event<CollisionEnterEvent> {
  uint64_t entityA{0};
  uint64_t entityB{0};
  float contactPointX{0};
  float contactPointY{0};
  float contactPointZ{0};
  float normalX{0};
  float normalY{0};
  float normalZ{0};
  float impulse{0};
};

/// @brief Fired on physics collision exit
struct CollisionExitEvent : Event<CollisionExitEvent> {
  uint64_t entityA{0};
  uint64_t entityB{0};
};

/// @brief Fired on trigger enter
struct TriggerEnterEvent : Event<TriggerEnterEvent> {
  uint64_t triggerEntity{0};
  uint64_t otherEntity{0};
};

/// @brief Fired on trigger exit
struct TriggerExitEvent : Event<TriggerExitEvent> {
  uint64_t triggerEntity{0};
  uint64_t otherEntity{0};
};

} // namespace Aetherion::Core
