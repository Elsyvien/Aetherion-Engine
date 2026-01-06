#pragma once

#include <memory>
#include <string>
#include <functional>
#include <QString>

#include <vector>
#include "Aetherion/Core/Types.h"

namespace Aetherion::Assets {
    class AssetRegistry;
}

namespace Aetherion::Scene {
    class Scene;
    class Entity; // Forward declaration for Entity
}

namespace Aetherion::Editor {

class Command;
class AICopilotAgent;

struct CopilotResult {
    QString response;
    std::vector<Core::EntityId> createdEntityIds;
    std::vector<QString> previewActions;
    bool dryRun{false};
    bool requestFocus{false};
    bool usedLLM{false};  // Whether LLM was used for this request
};

// LLM Configuration for the Copilot
struct CopilotLLMConfig {
    std::string endpoint{"http://localhost:11434/api/generate"};
    std::string model{"gpt-oss:20b"};
    std::string apiKey;
    bool enabled{true};
    float temperature{0.7f};
    int maxTokens{2048};
};

class AICopilotProcessor {
public:
    using CommandExecutor = std::function<void(std::unique_ptr<Command>)>;
    using EntityHighlightCallback = std::function<void(uint64_t entityId, float duration)>;
    using ActivityCallback = std::function<void(int activityType, const std::string& details)>;
    using ToolStatusCallback = std::function<void(const std::string& toolName, const std::string& params)>;

    AICopilotProcessor(CommandExecutor executor);
    ~AICopilotProcessor();

    void SetScene(std::shared_ptr<Scene::Scene> scene);
    void SetAssetRegistry(std::shared_ptr<Assets::AssetRegistry> registry);     
    void SetSelectedEntity(std::shared_ptr<Scene::Entity> selected);
    void SetHighlightCallback(const EntityHighlightCallback& callback) { m_highlightCallback = callback; }
    void SetActivityCallback(const ActivityCallback& callback) { m_activityCallback = callback; }
    void SetToolStatusCallback(const ToolStatusCallback& callback) { m_toolStatusCallback = callback; }
    
    // Configure LLM settings
    void ConfigureLLM(const CopilotLLMConfig& config);
    
    // Enable/disable LLM mode (falls back to pattern matching if disabled)
    void SetLLMEnabled(bool enabled);
    bool IsLLMEnabled() const { return m_llmConfig.enabled; }

    // Processes the prompt and returns a result
    CopilotResult ProcessPrompt(const QString& prompt, bool allowDryRun = true);
    
    // Process with LLM agent (agentic mode with tools)
    CopilotResult ProcessWithAgent(const QString& prompt);
    
    // Clear conversation history
    void ClearHistory();

private:
    std::shared_ptr<Scene::Scene> m_scene;
    std::shared_ptr<Assets::AssetRegistry> m_assetRegistry;
    std::shared_ptr<Scene::Entity> m_selectedEntity;
    CommandExecutor m_executor;
    EntityHighlightCallback m_highlightCallback;
    
    CopilotLLMConfig m_llmConfig;
    std::unique_ptr<AICopilotAgent> m_agent;
    ActivityCallback m_activityCallback;
    ToolStatusCallback m_toolStatusCallback;
    
    // Initialize agent with tools
    void InitializeAgent();
    
    // Pattern-based processing (fallback)
    CopilotResult ProcessWithPatterns(const QString& prompt, bool allowDryRun);
};

} // namespace Aetherion::Editor

