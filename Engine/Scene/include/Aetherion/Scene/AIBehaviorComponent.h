#pragma once

#include <string>

#include "Aetherion/Scene/Component.h"

namespace Aetherion::Scripting {
class ScriptingRuntime;
}

namespace Aetherion::Scene {

class AIBehaviorComponent final : public Component {
public:
    enum class ExecutionMode { Stub, LocalModel, RemoteService };

    AIBehaviorComponent();
    ~AIBehaviorComponent() override = default;

    [[nodiscard]] std::string GetDisplayName() const override;

    void SetPersonality(std::string personality);
    void SetKnowledgeBase(std::string knowledgeBase);
    void SetContext(std::string context);
    void SetExecutionMode(ExecutionMode mode) noexcept { m_mode = mode; }
    void SetDecisionInterval(float seconds) noexcept { m_decisionInterval = seconds; }
    void SetPromptAssetId(std::string assetId);
    void SetInlinePrompt(std::string prompt);

    [[nodiscard]] const std::string& GetCurrentState() const noexcept { return m_currentState; }
    [[nodiscard]] const std::string& GetLastReason() const noexcept { return m_lastReason; }
    [[nodiscard]] const std::string& GetPersonality() const noexcept { return m_personality; }
    [[nodiscard]] const std::string& GetKnowledgeBase() const noexcept { return m_knowledgeBase; }
    [[nodiscard]] const std::string& GetContext() const noexcept { return m_context; }
    [[nodiscard]] ExecutionMode GetExecutionMode() const noexcept { return m_mode; }
    [[nodiscard]] float GetDecisionInterval() const noexcept { return m_decisionInterval; }
    [[nodiscard]] const std::string& GetPromptAssetId() const noexcept { return m_promptAssetId; }
    [[nodiscard]] const std::string& GetInlinePrompt() const noexcept { return m_inlinePrompt; }

protected:
    void OnBeginPlay() override;
    void OnEndPlay() override;
    void OnUpdate(float deltaTime) override;

private:
    void RefreshScript();
    void EvaluateBehavior();

    std::string m_personality{"Default"};
    std::string m_knowledgeBase{"None"};
    std::string m_context;
    std::string m_currentState{"Idle"};
    std::string m_lastReason{"Uninitialized"};
    std::string m_promptAssetId;
    std::string m_inlinePrompt;
    ExecutionMode m_mode{ExecutionMode::Stub};
    float m_decisionInterval{0.5f};
    float m_timeSinceDecision{0.0f};
    bool m_dirtyPrompt{false};
};

} // namespace Aetherion::Scene
