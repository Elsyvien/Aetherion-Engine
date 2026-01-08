#pragma once

#include "Aetherion/Scene/Component.h"
#include <string>

namespace Aetherion::Scene {

class AIBehaviorComponent : public Component {
public:
  enum class ExecutionMode {
    Stub,         // Use simple keyword matching (default, no LLM)
    LocalModel,   // Use local Ollama model
    RemoteService // Use remote LLM service
  };

  AIBehaviorComponent();
  ~AIBehaviorComponent() override = default;

  [[nodiscard]] std::string GetDisplayName() const override;

  // Personality
  void SetPersonality(std::string personality);
  [[nodiscard]] const std::string &GetPersonality() const {
    return m_personality;
  }

  // Knowledge Base (asset ID or text)
  void SetKnowledgeBase(std::string kb);
  [[nodiscard]] const std::string &GetKnowledgeBase() const {
    return m_knowledgeBase;
  }

  // Execution Settings
  void SetExecutionMode(ExecutionMode mode) { m_mode = mode; }
  [[nodiscard]] ExecutionMode GetExecutionMode() const { return m_mode; }

  void SetDecisionInterval(float interval) { m_decisionInterval = interval; }
  [[nodiscard]] float GetDecisionInterval() const { return m_decisionInterval; }
  // Alias for compatibility
  float GetThinkingInterval() const { return m_decisionInterval; }

  // Prompting
  void SetPromptAssetId(std::string assetId);
  [[nodiscard]] const std::string &GetPromptAssetId() const {
    return m_promptAssetId;
  }

  void SetInlinePrompt(std::string prompt);
  [[nodiscard]] const std::string &GetInlinePrompt() const {
    return m_inlinePrompt;
  }

  // Runtime State
  void SetCurrentState(const std::string &state) { m_currentState = state; }
  [[nodiscard]] const std::string &GetCurrentState() const {
    return m_currentState;
  }

  void SetContext(std::string context);
  [[nodiscard]] const std::string &GetContext() const { return m_context; }

  [[nodiscard]] const std::string &GetLastReason() const {
    return m_lastReason;
  }

  // Runtime state (public for AIBehaviorSystem access)
  float m_timeSinceLastThought{0.0f};

protected:
  void OnBeginPlay() override;
  void OnEndPlay() override;
  void OnUpdate(float deltaTime) override;

private:
  void RefreshScript();
  void EvaluateBehavior();

  std::string m_personality{"Default Assistant"};
  std::string m_knowledgeBase;
  std::string m_context;

  std::string m_promptAssetId;
  std::string m_inlinePrompt;

  std::string m_currentState{"Idle"};
  std::string m_lastReason;

  ExecutionMode m_mode{ExecutionMode::Stub};
  float m_decisionInterval{2.0f}; // Seconds between AI updates
  float m_timeSinceDecision{0.0f};
  bool m_dirtyPrompt{false};
};

} // namespace Aetherion::Scene
