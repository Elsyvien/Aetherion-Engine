#include "Aetherion/Scene/AIBehaviorComponent.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>

#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Assets/LLMClient.h"
#include "Aetherion/Runtime/EngineContext.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scripting/ScriptingPlaceholder.h"
#include "nlohmann/json.hpp"

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

std::string LoadPromptText(const Aetherion::Assets::AssetRegistry* registry,
                           const std::string& assetId) {
    if (!registry || assetId.empty()) {
        return {};
    }
    const auto* entry = registry->FindEntry(assetId);
    if (!entry || entry->path.empty()) {
        return {};
    }

    std::ifstream input(entry->path);
    if (!input.is_open()) {
        return {};
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    const std::string contents = buffer.str();
    if (contents.empty()) {
        return {};
    }

    try {
        auto root = nlohmann::json::parse(contents);
        if (root.is_object() && root.contains("prompt") &&
            root["prompt"].is_string()) {
            return root["prompt"].get<std::string>();
        }
    } catch (...) {
    }

    return contents;
}

std::string BuildBehaviorSystemPrompt() {
    return R"(You are an AI behavior policy for the Aetherion game engine.
Return ONLY valid JSON with keys: state (string), reason (string).
State should be short (1-3 words) and stable across frames.
Prefer existing states when unsure.)";
}

std::string BuildBehaviorUserPrompt(const std::string& promptText,
                                    const std::string& contextJson,
                                    const std::string& currentState,
                                    const std::string& suggestedState) {
    std::ostringstream out;
    out << "Behavior prompt:\n"
        << (promptText.empty() ? "(none)" : promptText) << "\n\n";
    out << "Context JSON:\n" << contextJson << "\n\n";
    out << "Current state: " << currentState << "\n";
    if (!suggestedState.empty()) {
        out << "Suggested state: " << suggestedState << "\n";
    }
    return out.str();
}

bool ParseDecisionJson(const std::string& response,
                       std::string& outState,
                       std::string& outReason) {
    std::string payload = response;
    const auto start = response.find('{');
    const auto end = response.rfind('}');
    if (start != std::string::npos && end != std::string::npos && end > start) {
        payload = response.substr(start, end - start + 1);
    }

    auto data = nlohmann::json::parse(payload, nullptr, false);
    if (data.is_discarded() || !data.is_object()) {
        return false;
    }

    if (data.contains("state") && data["state"].is_string()) {
        outState = data["state"].get<std::string>();
    }
    if (data.contains("reason") && data["reason"].is_string()) {
        outReason = data["reason"].get<std::string>();
    }
    return !outState.empty();
}

Aetherion::Assets::ILLMClient* GetLocalOllamaClient() {
    static std::unique_ptr<Aetherion::Assets::ILLMClient> s_client;
    if (!s_client) {
        Aetherion::Assets::LLMConfig config;
        config.provider = Aetherion::Assets::LLMProvider::LocalOllama;
        config.endpoint = Aetherion::Assets::LLMConfig::GetDefaultEndpoint(
            config.provider);
        config.model = Aetherion::Assets::LLMConfig::GetDefaultModel(
            config.provider);
        s_client = Aetherion::Assets::LLMClientFactory::Create(config);
    }
    return s_client.get();
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

    std::uint64_t frameIndex = 0;
    if (auto* scene = GetScene()) {
        if (auto* context = scene->GetContext()) {
            frameIndex = context->GetFrameIndex();
            auto& budget = context->GetInferenceBudget();
            if (!budget.TryConsume(frameIndex)) {
                m_lastBudgetRemaining = budget.GetRemainingRequests();
                return;
            }
            m_lastBudgetRemaining = budget.GetRemainingRequests();
        }
    }

    m_timeSinceDecision = 0.0f;
    EvaluateBehavior();
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
    std::string stubState = "Idle";
    std::string stubReason = "Using stub policy.";

    if (ContainsCaseInsensitive(m_context, "enemy") ||
        ContainsCaseInsensitive(m_context, "threat")) {
        stubState = "Attack";
        stubReason = "Context mentions an enemy or threat.";
    } else if (ContainsCaseInsensitive(m_context, "player") ||
               ContainsCaseInsensitive(m_context, "target")) {
        stubState = "Chase";
        stubReason = "Context references a player/target.";
    } else if (ContainsCaseInsensitive(m_context, "patrol")) {
        stubState = "Patrol";
        stubReason = "Context requests patrol behavior.";
    } else if (ContainsCaseInsensitive(m_context, "idle")) {
        stubState = "Idle";
        stubReason = "Context explicitly requests idle state.";
    } else {
        stubState = "Idle";
        stubReason = "No explicit intent detected.";
    }

    std::string promptText;
    if (!m_promptAssetId.empty() && context) {
        if (const auto assets = context->GetAssetRegistry()) {
            promptText = LoadPromptText(assets.get(), m_promptAssetId);
        }
    }
    if (promptText.empty() && !m_inlinePrompt.empty()) {
        promptText = m_inlinePrompt;
    }

    nlohmann::json contextPayload;
    contextPayload["personality"] = m_personality;
    contextPayload["knowledge"] = m_knowledgeBase;
    contextPayload["context"] = m_context;
    contextPayload["currentState"] = m_currentState;
    contextPayload["decisionInterval"] = m_decisionInterval;
    if (!promptText.empty()) {
        contextPayload["prompt"] = promptText;
    }
    const std::string contextJson = contextPayload.dump();

    std::string nextState = stubState;
    std::string reason = stubReason;
    bool decided = false;
    m_lastInferenceMs = 0;

    if (m_mode == ExecutionMode::Stub) {
        reason += " (stub policy)";
        decided = true;
        m_lastInferenceSource = "Stub";
    } else {
        const std::string systemPrompt = BuildBehaviorSystemPrompt();
        const std::string userPrompt =
            BuildBehaviorUserPrompt(promptText, contextJson, m_currentState,
                                    stubState);
        const std::string schema = R"({
  "type": "object",
  "properties": {
    "state": {"type": "string"},
    "reason": {"type": "string"}
  },
  "required": ["state", "reason"]
})";

        if (m_mode == ExecutionMode::OnDevice) {
            auto backend = context ? context->GetOnDeviceInferenceBackend()
                                   : nullptr;
            if (backend && backend->IsReady()) {
                const auto response =
                    backend->EvaluateBehavior(systemPrompt, userPrompt, schema);
                if (response.success && !response.state.empty()) {
                    nextState = response.state;
                    reason = response.reason.empty()
                                 ? "On-device decision"
                                 : response.reason;
                    if (response.latencyMs > 0) {
                        reason += " (on-device " +
                                  std::to_string(response.latencyMs) + "ms)";
                    }
                    m_lastInferenceSource = "OnDevice";
                    m_lastInferenceMs = response.latencyMs;
                    decided = true;
                } else {
                    reason = response.errorMessage.empty()
                                 ? "On-device inference failed."
                                 : response.errorMessage;
                    m_lastInferenceSource = "OnDevice";
                }
            } else {
                reason = "On-device inference not available.";
                m_lastInferenceSource = "OnDevice";
            }
        } else {
            Assets::ILLMClient* llmClient = nullptr;
            if (m_mode == ExecutionMode::RemoteService) {
                if (context && context->HasAIConfig()) {
                    llmClient = context->GetAIClient();
                }
            } else if (m_mode == ExecutionMode::LocalModel) {
                if (context && context->HasAIConfig() &&
                    context->GetAIConfig().provider ==
                        Assets::LLMProvider::LocalOllama) {
                    llmClient = context->GetAIClient();
                }
                if (!llmClient) {
                    llmClient = GetLocalOllamaClient();
                }
            }

            if (llmClient && llmClient->IsReady()) {
                const auto response =
                    llmClient->GenerateJSON(systemPrompt, userPrompt, schema);
                if (response.success) {
                    std::string llmState;
                    std::string llmReason;
                    if (ParseDecisionJson(response.content, llmState, llmReason)) {
                        nextState = llmState;
                        reason = llmReason.empty() ? "LLM decision" : llmReason;
                        if (response.latencyMs > 0) {
                            reason += " (LLM " +
                                      std::to_string(response.latencyMs) + "ms)";
                        }
                        m_lastInferenceSource =
                            (m_mode == ExecutionMode::RemoteService)
                                ? "RemoteService"
                                : "LocalModel";
                        m_lastInferenceMs = response.latencyMs;
                        decided = true;
                    } else {
                        reason = "LLM response missing state; using fallback.";
                    }
                } else {
                    reason = "LLM request failed: " + response.errorMessage;
                }
            } else if (m_mode == ExecutionMode::RemoteService) {
                reason = "Remote LLM not configured.";
            } else {
                reason = "Local LLM not available.";
            }
        }

        if (!decided && context) {
            if (auto scripting = context->GetScriptingRuntime()) {
                const std::string scriptId = !m_promptAssetId.empty()
                                                 ? m_promptAssetId
                                                 : "inline_behavior";
                auto decision =
                    scripting->RunBehavior(scriptId, GetEntity(), contextJson);
                if (decision.success) {
                    nextState = decision.state;
                    reason = decision.reason;
                    m_lastInferenceSource = "Python";
                    decided = true;
                }
            }
        }

        if (!decided) {
            nextState = stubState;
            m_lastInferenceSource = "Stub";
            if (reason == stubReason) {
                reason += " (fallback)";
            } else {
                reason += " Falling back to stub: " + stubReason;
            }
        }
    }

    if (nextState != m_currentState) {
        m_currentState = nextState;
    }
    m_lastReason = reason;
}

} // namespace Aetherion::Scene
