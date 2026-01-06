#pragma once

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class QNetworkAccessManager;

namespace Aetherion::Editor {

// Tool definition for AI agent to use
struct ToolDefinition {
  std::string name;
  std::string description;
  nlohmann::json parameters; // JSON schema for parameters
  std::function<nlohmann::json(const nlohmann::json &)>
      execute; // Executes tool with params
};

// Message in conversation
struct Message {
  enum class Role { User, Assistant, System };
  Role role;
  std::string content;
  nlohmann::json toolCalls; // Tool calls made by assistant
};

// AI Agent configuration
struct AgentConfig {
  std::string model{"gpt-oss:20b"}; // Common local Ollama default
  std::string endpoint{"http://127.0.0.1:11434/api/generate"};
  float temperature{0.7f};
  int maxTokens{2048};
  int contextWindow{4096};
};

// Agentic AI Copilot
class AICopilotAgent {
public:
  AICopilotAgent(const AgentConfig &config = AgentConfig{});
  ~AICopilotAgent();

  // Configure LLM settings (can be called after construction)
  void Configure(const AgentConfig &config);

  // Register a tool the agent can use
  void RegisterTool(const ToolDefinition &tool);

  // Send message and get response
  std::string SendMessage(const std::string &userMessage);

  // Process multi-turn conversation with tool use
  std::string ProcessAgenticRequest(const std::string &userMessage);

  // Set system prompt
  void SetSystemPrompt(const std::string &prompt);

  // Get conversation history
  const std::vector<Message> &GetHistory() const { return m_history; }

  // Clear history
  void ClearHistory();

private:
  AgentConfig m_config;
  std::vector<Message> m_history;
  std::vector<ToolDefinition> m_tools;
  std::string m_systemPrompt;
  std::unique_ptr<QNetworkAccessManager> m_networkManager;

  // Helper methods
  std::string CallLLM(const std::string &prompt);
  nlohmann::json ParseLLMResponse(const std::string &response);
  nlohmann::json ExecuteToolCall(const std::string &toolName,
                                 const nlohmann::json &params);
  std::string BuildPromptWithContext();
  std::string FormatToolsAsContext() const;
  std::string CleanResponseForUser(const std::string &response);
};

} // namespace Aetherion::Editor
