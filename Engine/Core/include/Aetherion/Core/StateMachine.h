#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Aetherion::Core {

/// @brief Base interface for state machine states
/// @tparam Context User-defined context type passed to state callbacks
template <typename Context>
class IState {
public:
  virtual ~IState() = default;
  
  /// @brief Called when entering the state
  virtual void OnEnter(Context& context) {}
  
  /// @brief Called every update while in this state
  virtual void OnUpdate(Context& context, float deltaTime) {}
  
  /// @brief Called every fixed update (physics step)
  virtual void OnFixedUpdate(Context& context, float fixedDeltaTime) {}
  
  /// @brief Called when exiting the state
  virtual void OnExit(Context& context) {}
  
  /// @brief Get the state name (for debugging)
  [[nodiscard]] virtual std::string GetName() const = 0;
};

/// @brief Transition condition function type
template <typename Context>
using TransitionCondition = std::function<bool(const Context&)>;

/// @brief Represents a transition between states
template <typename Context>
struct Transition {
  std::string fromState;
  std::string toState;
  TransitionCondition<Context> condition;
  std::string name; // For debugging
};

/// @brief Hierarchical Finite State Machine with transitions and guards
///
/// @code
/// struct PlayerContext {
///   float health;
///   bool isGrounded;
///   glm::vec3 velocity;
/// };
///
/// // Define states
/// class IdleState : public IState<PlayerContext> {
/// public:
///   std::string GetName() const override { return "Idle"; }
///   void OnEnter(PlayerContext& ctx) override {
///     ctx.velocity = glm::vec3(0);
///   }
///   void OnUpdate(PlayerContext& ctx, float dt) override {
///     // Idle animation
///   }
/// };
///
/// // Create state machine
/// StateMachine<PlayerContext> fsm;
/// fsm.AddState<IdleState>();
/// fsm.AddState<RunningState>();
/// fsm.AddState<JumpingState>();
///
/// // Add transitions
/// fsm.AddTransition("Idle", "Running", [](const PlayerContext& ctx) {
///   return glm::length(ctx.velocity) > 0.1f;
/// });
///
/// fsm.AddTransition("Running", "Jumping", [](const PlayerContext& ctx) {
///   return !ctx.isGrounded;
/// });
///
/// // Set initial state
/// fsm.SetState("Idle");
///
/// // In game loop
/// fsm.Update(playerContext, deltaTime);
/// @endcode
template <typename Context>
class StateMachine {
public:
  using StatePtr = std::unique_ptr<IState<Context>>;
  using StateFactory = std::function<StatePtr()>;
  
  StateMachine() = default;
  
  // ===========================================================================
  // State Management
  // ===========================================================================
  
  /// @brief Add a state using its default constructor
  template <typename StateType, typename... Args>
  void AddState(Args&&... args) {
    static_assert(std::is_base_of_v<IState<Context>, StateType>,
                  "StateType must derive from IState<Context>");
    
    auto state = std::make_unique<StateType>(std::forward<Args>(args)...);
    std::string name = state->GetName();
    m_states[name] = std::move(state);
  }
  
  /// @brief Add a state with a custom name
  template <typename StateType, typename... Args>
  void AddState(const std::string& name, Args&&... args) {
    auto state = std::make_unique<StateType>(std::forward<Args>(args)...);
    m_states[name] = std::move(state);
  }
  
  /// @brief Add a state instance directly
  void AddState(const std::string& name, StatePtr state) {
    m_states[name] = std::move(state);
  }
  
  /// @brief Remove a state
  void RemoveState(const std::string& name) {
    if (m_currentStateName == name) {
      m_currentState = nullptr;
      m_currentStateName.clear();
    }
    m_states.erase(name);
    
    // Remove transitions involving this state
    m_transitions.erase(
      std::remove_if(m_transitions.begin(), m_transitions.end(),
        [&name](const Transition<Context>& t) {
          return t.fromState == name || t.toState == name;
        }),
      m_transitions.end()
    );
  }
  
  /// @brief Check if a state exists
  [[nodiscard]] bool HasState(const std::string& name) const {
    return m_states.contains(name);
  }
  
  /// @brief Get number of states
  [[nodiscard]] size_t GetStateCount() const { return m_states.size(); }
  
  // ===========================================================================
  // Transitions
  // ===========================================================================
  
  /// @brief Add a conditional transition between states
  void AddTransition(const std::string& from, const std::string& to,
                     TransitionCondition<Context> condition,
                     const std::string& name = "") {
    m_transitions.push_back(Transition<Context>{
      from, to, std::move(condition), name
    });
  }
  
  /// @brief Add a transition that can be triggered from any state
  void AddGlobalTransition(const std::string& to,
                           TransitionCondition<Context> condition,
                           const std::string& name = "") {
    m_globalTransitions.push_back(Transition<Context>{
      "*", to, std::move(condition), name
    });
  }
  
  /// @brief Remove all transitions from a state
  void RemoveTransitionsFrom(const std::string& from) {
    m_transitions.erase(
      std::remove_if(m_transitions.begin(), m_transitions.end(),
        [&from](const Transition<Context>& t) { return t.fromState == from; }),
      m_transitions.end()
    );
  }
  
  // ===========================================================================
  // State Control
  // ===========================================================================
  
  /// @brief Set the current state immediately
  void SetState(const std::string& name, Context& context) {
    auto it = m_states.find(name);
    if (it == m_states.end()) return;
    
    // Exit current state
    if (m_currentState) {
      m_currentState->OnExit(context);
    }
    
    m_previousStateName = m_currentStateName;
    m_currentStateName = name;
    m_currentState = it->second.get();
    
    // Enter new state
    m_currentState->OnEnter(context);
    
    // Notify listeners
    for (const auto& callback : m_stateChangeCallbacks) {
      callback(m_previousStateName, m_currentStateName);
    }
  }
  
  /// @brief Force set state without calling OnExit/OnEnter (for initialization)
  void ForceState(const std::string& name) {
    auto it = m_states.find(name);
    if (it == m_states.end()) return;
    
    m_previousStateName = m_currentStateName;
    m_currentStateName = name;
    m_currentState = it->second.get();
  }
  
  /// @brief Get current state name
  [[nodiscard]] const std::string& GetCurrentStateName() const {
    return m_currentStateName;
  }
  
  /// @brief Get previous state name
  [[nodiscard]] const std::string& GetPreviousStateName() const {
    return m_previousStateName;
  }
  
  /// @brief Get current state
  [[nodiscard]] IState<Context>* GetCurrentState() {
    return m_currentState;
  }
  
  /// @brief Get a state by name
  [[nodiscard]] IState<Context>* GetState(const std::string& name) {
    auto it = m_states.find(name);
    return it != m_states.end() ? it->second.get() : nullptr;
  }
  
  // ===========================================================================
  // Update
  // ===========================================================================
  
  /// @brief Update the state machine (call every frame)
  void Update(Context& context, float deltaTime) {
    if (!m_currentState) return;
    
    // Check global transitions first
    for (const auto& transition : m_globalTransitions) {
      if (transition.toState != m_currentStateName && 
          transition.condition(context)) {
        SetState(transition.toState, context);
        return;
      }
    }
    
    // Check state-specific transitions
    for (const auto& transition : m_transitions) {
      if (transition.fromState == m_currentStateName && 
          transition.condition(context)) {
        SetState(transition.toState, context);
        return;
      }
    }
    
    // Update current state
    m_currentState->OnUpdate(context, deltaTime);
  }
  
  /// @brief Fixed update for physics (call every fixed timestep)
  void FixedUpdate(Context& context, float fixedDeltaTime) {
    if (m_currentState) {
      m_currentState->OnFixedUpdate(context, fixedDeltaTime);
    }
  }
  
  // ===========================================================================
  // Events
  // ===========================================================================
  
  /// @brief Register a callback for state changes
  void OnStateChange(std::function<void(const std::string&, const std::string&)> callback) {
    m_stateChangeCallbacks.push_back(std::move(callback));
  }
  
  // ===========================================================================
  // Debugging
  // ===========================================================================
  
  /// @brief Get all state names
  [[nodiscard]] std::vector<std::string> GetStateNames() const {
    std::vector<std::string> names;
    names.reserve(m_states.size());
    for (const auto& [name, state] : m_states) {
      names.push_back(name);
    }
    return names;
  }
  
  /// @brief Get all transitions
  [[nodiscard]] const std::vector<Transition<Context>>& GetTransitions() const {
    return m_transitions;
  }

private:
  std::unordered_map<std::string, StatePtr> m_states;
  std::vector<Transition<Context>> m_transitions;
  std::vector<Transition<Context>> m_globalTransitions;
  
  IState<Context>* m_currentState{nullptr};
  std::string m_currentStateName;
  std::string m_previousStateName;
  
  std::vector<std::function<void(const std::string&, const std::string&)>> m_stateChangeCallbacks;
};

// =============================================================================
// Lambda-based State for quick prototyping
// =============================================================================

/// @brief State implementation using lambdas (no need to define classes)
template <typename Context>
class LambdaState : public IState<Context> {
public:
  using EnterFunc = std::function<void(Context&)>;
  using UpdateFunc = std::function<void(Context&, float)>;
  using ExitFunc = std::function<void(Context&)>;
  
  explicit LambdaState(std::string name,
                       EnterFunc onEnter = nullptr,
                       UpdateFunc onUpdate = nullptr,
                       ExitFunc onExit = nullptr)
    : m_name(std::move(name))
    , m_onEnter(std::move(onEnter))
    , m_onUpdate(std::move(onUpdate))
    , m_onExit(std::move(onExit)) {}
  
  [[nodiscard]] std::string GetName() const override { return m_name; }
  
  void OnEnter(Context& context) override {
    if (m_onEnter) m_onEnter(context);
  }
  
  void OnUpdate(Context& context, float deltaTime) override {
    if (m_onUpdate) m_onUpdate(context, deltaTime);
  }
  
  void OnExit(Context& context) override {
    if (m_onExit) m_onExit(context);
  }
  
private:
  std::string m_name;
  EnterFunc m_onEnter;
  UpdateFunc m_onUpdate;
  ExitFunc m_onExit;
};

/// @brief Helper to create lambda states inline
template <typename Context>
[[nodiscard]] std::unique_ptr<LambdaState<Context>> MakeLambdaState(
    std::string name,
    typename LambdaState<Context>::EnterFunc onEnter = nullptr,
    typename LambdaState<Context>::UpdateFunc onUpdate = nullptr,
    typename LambdaState<Context>::ExitFunc onExit = nullptr) {
  return std::make_unique<LambdaState<Context>>(
    std::move(name),
    std::move(onEnter),
    std::move(onUpdate),
    std::move(onExit)
  );
}

// =============================================================================
// Pushdown Automaton (Stack-based State Machine)
// =============================================================================

/// @brief Stack-based state machine for nested states (pause menus, dialogs, etc.)
///
/// @code
/// PushdownAutomaton<GameContext> pda;
/// pda.AddState<PlayingState>();
/// pda.AddState<PausedState>();
/// pda.AddState<InventoryState>();
///
/// pda.Push("Playing", context);
///
/// // Pause game (keeps playing state underneath)
/// pda.Push("Paused", context);
///
/// // Open inventory on top of pause
/// pda.Push("Inventory", context);
///
/// // Close inventory, back to pause
/// pda.Pop(context);
///
/// // Resume game
/// pda.Pop(context);
/// @endcode
template <typename Context>
class PushdownAutomaton {
public:
  using StatePtr = std::unique_ptr<IState<Context>>;
  
  /// @brief Add a state
  template <typename StateType, typename... Args>
  void AddState(Args&&... args) {
    auto state = std::make_unique<StateType>(std::forward<Args>(args)...);
    std::string name = state->GetName();
    m_states[name] = std::move(state);
  }
  
  /// @brief Add a state with explicit name
  void AddState(const std::string& name, StatePtr state) {
    m_states[name] = std::move(state);
  }
  
  /// @brief Push a new state onto the stack
  void Push(const std::string& name, Context& context) {
    auto it = m_states.find(name);
    if (it == m_states.end()) return;
    
    // Pause current state (don't exit)
    if (!m_stack.empty() && m_pauseHandler) {
      m_pauseHandler(m_stack.back(), context);
    }
    
    m_stack.push_back(name);
    it->second->OnEnter(context);
  }
  
  /// @brief Pop the current state off the stack
  void Pop(Context& context) {
    if (m_stack.empty()) return;
    
    auto it = m_states.find(m_stack.back());
    if (it != m_states.end()) {
      it->second->OnExit(context);
    }
    m_stack.pop_back();
    
    // Resume previous state (don't re-enter)
    if (!m_stack.empty() && m_resumeHandler) {
      m_resumeHandler(m_stack.back(), context);
    }
  }
  
  /// @brief Replace current state (pop + push)
  void Replace(const std::string& name, Context& context) {
    if (!m_stack.empty()) {
      auto it = m_states.find(m_stack.back());
      if (it != m_states.end()) {
        it->second->OnExit(context);
      }
      m_stack.pop_back();
    }
    Push(name, context);
  }
  
  /// @brief Pop all states and push a new one
  void PopAll(Context& context) {
    while (!m_stack.empty()) {
      auto it = m_states.find(m_stack.back());
      if (it != m_states.end()) {
        it->second->OnExit(context);
      }
      m_stack.pop_back();
    }
  }
  
  /// @brief Update the top state
  void Update(Context& context, float deltaTime) {
    if (m_stack.empty()) return;
    
    auto it = m_states.find(m_stack.back());
    if (it != m_states.end()) {
      it->second->OnUpdate(context, deltaTime);
    }
  }
  
  /// @brief Get current state name
  [[nodiscard]] std::string GetCurrentStateName() const {
    return m_stack.empty() ? "" : m_stack.back();
  }
  
  /// @brief Get stack depth
  [[nodiscard]] size_t GetStackDepth() const { return m_stack.size(); }
  
  /// @brief Check if a state is in the stack
  [[nodiscard]] bool IsInStack(const std::string& name) const {
    return std::find(m_stack.begin(), m_stack.end(), name) != m_stack.end();
  }
  
  /// @brief Set handlers for pause/resume
  void SetPauseHandler(std::function<void(const std::string&, Context&)> handler) {
    m_pauseHandler = std::move(handler);
  }
  
  void SetResumeHandler(std::function<void(const std::string&, Context&)> handler) {
    m_resumeHandler = std::move(handler);
  }

private:
  std::unordered_map<std::string, StatePtr> m_states;
  std::vector<std::string> m_stack;
  std::function<void(const std::string&, Context&)> m_pauseHandler;
  std::function<void(const std::string&, Context&)> m_resumeHandler;
};

} // namespace Aetherion::Core
