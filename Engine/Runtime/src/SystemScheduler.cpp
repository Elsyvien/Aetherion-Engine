#include "Aetherion/Runtime/SystemScheduler.h"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <stdexcept>

namespace Aetherion::Runtime {

void SystemScheduler::RegisterSystem(std::shared_ptr<IRuntimeSystem> system,
                                     SystemPhase phase, SystemPriority priority,
                                     std::vector<std::string> dependencies) {
  if (!system) {
    return;
  }

  const std::string name = system->GetName();
  if (name.empty()) {
    return;
  }

  SystemInfo info;
  info.system = std::move(system);
  info.phase = phase;
  info.priority = priority;
  info.dependencies = std::move(dependencies);
  info.enabled = true;
  info.initialized = false;

  m_systems[name] = std::move(info);
  m_orderDirty = true;
  m_orderValid = false;
}

void SystemScheduler::UnregisterSystem(const std::string &name) {
  auto it = m_systems.find(name);
  if (it == m_systems.end()) {
    return;
  }

  m_systems.erase(it);
  m_orderDirty = true;
  m_orderValid = false;

  // Remove from execution order
  m_executionOrder.erase(
      std::remove(m_executionOrder.begin(), m_executionOrder.end(), name),
      m_executionOrder.end());
}

void SystemScheduler::SetSystemEnabled(const std::string &name, bool enabled) {
  auto it = m_systems.find(name);
  if (it != m_systems.end()) {
    it->second.enabled = enabled;
  }
}

bool SystemScheduler::IsSystemEnabled(const std::string &name) const {
  auto it = m_systems.find(name);
  return it != m_systems.end() && it->second.enabled;
}

const SystemInfo *
SystemScheduler::GetSystemInfo(const std::string &name) const {
  auto it = m_systems.find(name);
  return it != m_systems.end() ? &it->second : nullptr;
}

bool SystemScheduler::BuildExecutionOrder() {
  if (!m_orderDirty && m_orderValid) {
    return true;
  }

  m_executionOrder.clear();
  m_cycleDiagnostic.clear();
  m_orderValid = TopologicalSort();
  m_orderDirty = false;

  return m_orderValid;
}

bool SystemScheduler::TopologicalSort() {
  // Group systems by phase first
  std::vector<std::vector<DependencyNode>>
      phaseGroups(static_cast<size_t>(SystemPhase::Count));

  for (const auto &[name, info] : m_systems) {
    DependencyNode node;
    node.name = name;
    node.phase = info.phase;
    node.priority = info.priority;
    node.dependencies = info.dependencies;
    node.enabled = info.enabled;

    phaseGroups[static_cast<size_t>(info.phase)].push_back(std::move(node));
  }

  // Sort each phase by priority, then by name for determinism
  for (auto &group : phaseGroups) {
    std::sort(group.begin(), group.end(),
              [](const DependencyNode &a, const DependencyNode &b) {
                if (a.priority != b.priority) {
                  return a.priority < b.priority;
                }
                return a.name < b.name; // Alphabetical for determinism
              });
  }

  // Build dependency graph for topological sort within each phase
  std::unordered_set<std::string> visiting;
  std::unordered_set<std::string> visited;
  std::vector<std::string> result;

  // Process phases in order
  for (size_t phaseIdx = 0; phaseIdx < static_cast<size_t>(SystemPhase::Count);
       ++phaseIdx) {
    const auto &group = phaseGroups[phaseIdx];

    // For each phase, do a topological sort respecting dependencies
    std::vector<std::string> phaseResult;
    std::unordered_set<std::string> phaseVisiting;
    std::unordered_set<std::string> phaseVisited;

    // Build adjacency list for this phase
    std::unordered_map<std::string, std::vector<std::string>> adj;
    std::unordered_set<std::string> phaseNodes;

    for (const auto &node : group) {
      phaseNodes.insert(node.name);
      adj[node.name]; // Ensure entry exists
    }

    // Add edges for dependencies within this phase
    for (const auto &node : group) {
      for (const auto &dep : node.dependencies) {
        // Only add if dependency is in the same phase
        if (phaseNodes.count(dep)) {
          adj[dep].push_back(node.name);
        }
      }
    }

    // Topological sort using DFS
    std::vector<std::string> sortedPhase;
    for (const auto &node : group) {
      if (!phaseVisited.count(node.name)) {
        if (DetectCycle(node.name, phaseVisiting, phaseVisited, sortedPhase)) {
          return false;
        }
      }
    }

    // Reverse to get correct order
    std::reverse(sortedPhase.begin(), sortedPhase.end());

    // Re-sort by priority while respecting dependencies
    // Simple approach: the topological sort already handles deps,
    // but we need to maintain priority order where possible
    std::stable_sort(
        sortedPhase.begin(), sortedPhase.end(),
        [this](const std::string &a, const std::string &b) {
          const auto *infoA = GetSystemInfo(a);
          const auto *infoB = GetSystemInfo(b);
          if (infoA && infoB) {
            if (infoA->priority != infoB->priority) {
              return infoA->priority < infoB->priority;
            }
          }
          return a < b;
        });

    // Add to final result
    for (const auto &name : sortedPhase) {
      result.push_back(name);
    }
  }

  m_executionOrder = std::move(result);
  return true;
}

bool SystemScheduler::DetectCycle(const std::string &node,
                                  std::unordered_set<std::string> &visiting,
                                  std::unordered_set<std::string> &visited,
                                  std::vector<std::string> &result) {
  if (visiting.count(node)) {
    // Cycle detected
    std::ostringstream oss;
    oss << "Dependency cycle detected involving system: " << node;
    m_cycleDiagnostic = oss.str();
    return true;
  }

  if (visited.count(node)) {
    return false;
  }

  visiting.insert(node);

  // Check dependencies
  auto it = m_systems.find(node);
  if (it != m_systems.end()) {
    for (const auto &dep : it->second.dependencies) {
      if (m_systems.count(dep)) {
        if (DetectCycle(dep, visiting, visited, result)) {
          return true;
        }
      }
    }
  }

  visiting.erase(node);
  visited.insert(node);
  result.push_back(node);

  return false;
}

void SystemScheduler::InitializeSystems(EngineContext &context) {
  if (!m_orderValid) {
    BuildExecutionOrder();
  }

  for (const auto &name : m_executionOrder) {
    auto it = m_systems.find(name);
    if (it != m_systems.end() && it->second.enabled && !it->second.initialized) {
      it->second.system->Initialize(context);
      it->second.initialized = true;
    }
  }
}

void SystemScheduler::TickSystems(EngineContext &context, float deltaTime) {
  if (!m_orderValid) {
    BuildExecutionOrder();
  }

  for (const auto &name : m_executionOrder) {
    auto it = m_systems.find(name);
    if (it == m_systems.end() || !it->second.enabled) {
      continue;
    }

    if (m_profileCallback) {
      auto start = std::chrono::high_resolution_clock::now();
      it->second.system->Tick(context, deltaTime);
      auto end = std::chrono::high_resolution_clock::now();
      float durationMs = std::chrono::duration<float, std::milli>(end - start).count();
      m_profileCallback(name, durationMs);
    } else {
      it->second.system->Tick(context, deltaTime);
    }
  }
}

void SystemScheduler::TickPhase(EngineContext &context, float deltaTime,
                                SystemPhase phase) {
  if (!m_orderValid) {
    BuildExecutionOrder();
  }

  for (const auto &name : m_executionOrder) {
    auto it = m_systems.find(name);
    if (it == m_systems.end() || !it->second.enabled) {
      continue;
    }

    if (it->second.phase != phase) {
      continue;
    }

    if (m_profileCallback) {
      auto start = std::chrono::high_resolution_clock::now();
      it->second.system->Tick(context, deltaTime);
      auto end = std::chrono::high_resolution_clock::now();
      float durationMs = std::chrono::duration<float, std::milli>(end - start).count();
      m_profileCallback(name, durationMs);
    } else {
      it->second.system->Tick(context, deltaTime);
    }
  }
}

void SystemScheduler::ShutdownSystems(EngineContext &context) {
  // Shutdown in reverse order
  for (auto it = m_executionOrder.rbegin(); it != m_executionOrder.rend();
       ++it) {
    auto sysIt = m_systems.find(*it);
    if (sysIt != m_systems.end() && sysIt->second.initialized) {
      sysIt->second.system->Shutdown(context);
      sysIt->second.initialized = false;
    }
  }
}

std::vector<std::string> SystemScheduler::GetRegisteredSystemNames() const {
  std::vector<std::string> names;
  names.reserve(m_systems.size());
  for (const auto &[name, info] : m_systems) {
    names.push_back(name);
  }
  std::sort(names.begin(), names.end());
  return names;
}

std::vector<std::vector<std::string>>
SystemScheduler::GetSystemsByPhase() const {
  std::vector<std::vector<std::string>> result(
      static_cast<size_t>(SystemPhase::Count));

  for (const auto &name : m_executionOrder) {
    auto it = m_systems.find(name);
    if (it != m_systems.end()) {
      result[static_cast<size_t>(it->second.phase)].push_back(name);
    }
  }

  return result;
}

} // namespace Aetherion::Runtime
