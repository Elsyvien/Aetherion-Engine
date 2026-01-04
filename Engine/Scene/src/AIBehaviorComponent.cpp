#include "Aetherion/Scene/AIBehaviorComponent.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Runtime/EngineContext.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scripting/ScriptingPlaceholder.h"

namespace {
bool ContainsCaseInsensitive(const std::string& haystack,
                             const std::string& needle) {
    if (needle.empty() || haystack.size() < needle.size()) {
        return false;
    }
    auto it = std::search(
        haystack.begin(), haystack.end(), needle.begin(), needle.end(),
        [](char a, char b) { return std::tolower(a) == std::tolower(b); });
    return it != haystack.end();
}
} // namespace

namespace Aetherion::Scene {

AIBehaviorComponent::AIBehaviorComponent() = default;

std::string AIBehaviorComponent::GetDisplayName() const {
    return "AI Behavior";
}

void AIBehaviorComponent::SetPersonality(std::string personality) {
    m_personality = std::move(personality);
    m_dirtyPrompt = true;
}

void AIBehaviorComponent::SetKnowledgeBase(std::string knowledgeBase) {
    m_knowledgeBase = std::move(knowledgeBase);
}

void AIBehaviorComponent::SetContext(std::string context) {
    m_context = std::move(context);
}

void AIBehaviorComponent::SetPromptAssetId(std::string assetId) {
    m_promptAssetId = std::move(assetId);
    m_dirtyPrompt = true;
}

void AIBehaviorComponent::SetInlinePrompt(std::string prompt) {
    m_inlinePrompt = std::move(prompt);
    m_dirtyPrompt = true;
}

void AIBehaviorComponent::OnBeginPlay() {
    m_timeSinceDecision = m_decisionInterval;
    m_dirtyPrompt = true;
    RefreshScript();
}

void AIBehaviorComponent::OnEndPlay() {
    m_currentState = "Idle";
    m_lastReason = "Stopped";
}

void AIBehaviorComponent::OnUpdate(float deltaTime) {
    m_timeSinceDecision += deltaTime;
    if (m_dirtyPrompt) {
        RefreshScript();
    }
    if (m_timeSinceDecision < m_decisionInterval) {
        return;
    }

    static int s_budgetCounter = 0;
    constexpr int kBudgetPerFrame = 8;
    if (s_budgetCounter >= kBudgetPerFrame) {
        return;
    }
    ++s_budgetCounter;

    m_timeSinceDecision = 0.0f;
    EvaluateBehavior();
    s_budgetCounter = 0;
}

void AIBehaviorComponent::RefreshScript() {
    m_dirtyPrompt = false;
    Scene* scene = GetScene();
    if (!scene) {
        return;
    }

    auto* context = scene->GetContext();
    if (!context) {
        return;
    }

    auto scripting = context->GetScriptingRuntime();
    if (!scripting) {
        return;
    }

    if (!m_promptAssetId.empty()) {
        if (const auto assets = context->GetAssetRegistry()) {
            if (const auto* entry = assets->FindEntry(m_promptAssetId)) {
                scripting->RegisterPromptAsset(entry->id, entry->path);
            }
        }
    } else if (!m_inlinePrompt.empty()) {
        scripting->RegisterInlinePrompt(m_promptAssetId.empty()
                                            ? "inline_behavior"
                                            : m_promptAssetId,
                                        m_inlinePrompt, "InlineBehavior");
    }
}

void AIBehaviorComponent::EvaluateBehavior() {
    Scene* scene = GetScene();
    Runtime::EngineContext* context = scene ? scene->GetContext() : nullptr;

    std::string nextState = m_currentState;
    std::string reason = "Using stub policy.";

    if (ContainsCaseInsensitive(m_context, "enemy") ||
        ContainsCaseInsensitive(m_context, "threat")) {
        nextState = "Attack";
        reason = "Context mentions an enemy or threat.";
    } else if (ContainsCaseInsensitive(m_context, "player") ||
               ContainsCaseInsensitive(m_context, "target")) {
        nextState = "Chase";
        reason = "Context references a player/target.";
    } else if (ContainsCaseInsensitive(m_context, "patrol")) {
        nextState = "Patrol";
        reason = "Context requests patrol behavior.";
    } else if (ContainsCaseInsensitive(m_context, "idle")) {
        nextState = "Idle";
        reason = "Context explicitly requests idle state.";
    } else {
        nextState = "Idle";
        reason = "No explicit intent detected.";
    }

    if (m_mode != ExecutionMode::Stub && context) {
        if (auto scripting = context->GetScriptingRuntime()) {
            std::ostringstream ctxJson;
            ctxJson << "{";
            ctxJson << "\"personality\":\"" << m_personality << "\",";
            ctxJson << "\"knowledge\":\"" << m_knowledgeBase << "\",";
            ctxJson << "\"context\":\"" << m_context << "\"";
            ctxJson << "}";

            const std::string scriptId =
                !m_promptAssetId.empty() ? m_promptAssetId : "inline_behavior";
            auto decision = scripting->RunBehavior(scriptId, ctxJson.str());
            nextState = decision.state;
            reason = decision.reason;
        }
    } else {
        reason += " (Model execution not wired yet; stubbed.)";
    }

    if (nextState != m_currentState) {
        m_currentState = nextState;
    }
    m_lastReason = reason;
}

} // namespace Aetherion::Scene
