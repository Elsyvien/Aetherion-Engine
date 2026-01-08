#include "Aetherion/Editor/AICopilotAgent.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <regex>
#include <sstream>


#include <QByteArray>
#include <QDebug>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace Aetherion::Editor {

using json = nlohmann::json;

AICopilotAgent::AICopilotAgent(const AgentConfig &config)
    : m_config(config),
      m_networkManager(std::make_unique<QNetworkAccessManager>()) {
  // Set default system prompt for Aetherion Engine
  m_systemPrompt =
      R"(You are an AI assistant for the Aetherion Game Engine. You help users create, modify, and manage game entities, scenes, and components. You can also generate and attach scripts to entities.

You have access to tools that let you interact with the engine. When a user asks you to create, spawn, move, modify, or delete something in the scene, YOU MUST IMMEDIATELY USE THE APPROPRIATE TOOL.

CRITICAL TOOL USAGE RULES:
1. ALWAYS use tools when the user requests scene manipulation - DO NOT just describe what you would do
2. Output tool calls in JSON blocks with THREE BACKTICKS:

```tool
{
    "tool": "spawn_entity",
    "params": {"type": "cube", "name": "MyEntity", "position": {"x": 0, "y": 0, "z": 0}}
}
```

3. After EACH tool call, wait for the result and acknowledge it
4. For multiple actions, use multiple tool blocks
5. Example sequences:
   - User: "Make a cube" → spawn_entity tool call → confirm creation
   - User: "Make it spin" → generate_script with rotation → attach_script to entity

SCRIPTING CAPABILITIES:
- Use generate_script for simple behaviors (rotation, movement, bounce) - generates Lua
- For complex systems, suggest the Logic Copilot panel for C++ code generation
- Use attach_script to add scripts to entities
- Language selection:
  * LUA: Simple entity behaviors like rotation, movement, bounce, float, blink
  * C++: Complex systems, multi-entity coordination, physics, pathfinding

AVAILABLE ENTITY TYPES:
- Cube, Sphere, Cylinder, Plane, Cone, Pyramid (primitives)
- Light, Camera, Empty (special types)

AVAILABLE TOOLS (USE THESE FOR SCENE MANIPULATION):
- spawn_entity: Create a new entity in the scene
- add_component: Add a component to an entity
- modify_entity: Change entity properties
- list_scene_entities: Show all entities
- generate_script: Generate Lua/C++ script from description (use language: auto, lua, or cpp)
- attach_script: Attach a script to an entity
- list_scripts: Show available script templates

WHEN USER REQUESTS SCENE CHANGES: FIRST OUTPUT TOOL BLOCKS, THEN EXPLAIN RESULTS)";
}

AICopilotAgent::~AICopilotAgent() = default;

void AICopilotAgent::Configure(const AgentConfig &config) { m_config = config; }

void AICopilotAgent::RegisterTool(const ToolDefinition &tool) {
  m_tools.push_back(tool);
}

void AICopilotAgent::SetSystemPrompt(const std::string &prompt) {
  m_systemPrompt = prompt;
}

void AICopilotAgent::ClearHistory() { m_history.clear(); }

std::string AICopilotAgent::FormatToolsAsContext() const {
  std::stringstream ss;
  ss << "\n=== AVAILABLE TOOLS (USE THESE!) ===\n";
  ss << "Format tool calls as JSON in ```tool ... ``` blocks\n";
  ss << "Each tool block must have \"tool\" and \"params\" fields\n\n";

  for (const auto &tool : m_tools) {
    ss << "TOOL: " << tool.name << "\n";
    ss << "Description: " << tool.description << "\n";
    ss << "Parameters:\n" << tool.parameters.dump(2) << "\n\n";
  }

  ss << "=== USAGE EXAMPLE ===\n";
  ss << "When user says 'make a cube':\n";
  ss << "```tool\n";
  ss << "{\n";
  ss << "  \"tool\": \"spawn_entity\",\n";
  ss << "  \"params\": {\"type\": \"cube\", \"name\": \"Cube1\"}\n";
  ss << "}\n";
  ss << "```\n";

  return ss.str();
}

std::string AICopilotAgent::BuildPromptWithContext() {
  std::stringstream prompt;

  // System prompt
  prompt << "### System:\n" << m_systemPrompt << "\n";
  prompt << FormatToolsAsContext() << "\n";

  // Conversation history
  prompt << "\n### Conversation:\n";
  for (const auto &msg : m_history) {
    switch (msg.role) {
    case Message::Role::User:
      prompt << "User: " << msg.content << "\n";
      break;
    case Message::Role::Assistant:
      prompt << "Assistant: " << msg.content << "\n";
      if (!msg.toolCalls.empty()) {
        prompt << "[Tool calls: " << msg.toolCalls.dump() << "]\n";
      }
      break;
    case Message::Role::System:
      prompt << "[System: " << msg.content << "]\n";
      break;
    }
  }

  prompt << "Assistant: ";
  return prompt.str();
}

std::string AICopilotAgent::CallLLM(const std::string &prompt) {
  // Build request for Ollama /api/generate (prompt) or chat-style endpoints
  json request;
  request["model"] = m_config.model;
  request["stream"] = false;
  request["max_tokens"] = m_config.maxTokens; // OpenAI-compatible endpoints

  // Normalize endpoint to a concrete URL
  std::string requestUrl = m_config.endpoint;
  if (requestUrl.empty()) {
    requestUrl = "http://localhost:11434";
  }
  while (!requestUrl.empty() && requestUrl.back() == '/') {
    requestUrl.pop_back();
  }

  const bool hasGenerate =
      requestUrl.size() >= 13 &&
      requestUrl.rfind("/api/generate") == requestUrl.size() - 13;
  const bool hasChatApi =
      requestUrl.size() >= 9 &&
      requestUrl.rfind("/api/chat") == requestUrl.size() - 9;
  const bool hasChatCompletions =
      requestUrl.size() >= 18 &&
      requestUrl.rfind("/chat/completions") == requestUrl.size() - 18;

  bool useGenerate = hasGenerate;

  if (!hasGenerate && !hasChatApi && !hasChatCompletions) {
    // If user gave base or /v1, append OpenAI-compatible chat endpoint
    const bool endsWithV1 = requestUrl.size() >= 3 &&
                            requestUrl.rfind("/v1") == requestUrl.size() - 3;
    if (endsWithV1) {
      requestUrl += "/chat/completions";
    } else {
      requestUrl += "/v1/chat/completions";
    }
  }

  if (useGenerate) {
    request["prompt"] = prompt;
  } else {
    json messages = json::array();
    json userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = prompt;
    messages.push_back(userMsg);
    request["messages"] = messages;
  }

  // Ollama-specific options
  request["options"]["temperature"] = m_config.temperature;
  request["options"]["num_predict"] = m_config.maxTokens;
  request["options"]["num_ctx"] = m_config.contextWindow;

  std::string requestBody = request.dump();

  // Use Qt's QNetworkAccessManager for HTTP request
  QNetworkRequest httpRequest(QUrl(QString::fromStdString(requestUrl)));
  httpRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  qDebug() << "[AICopilot] Sending request to:"
           << QString::fromStdString(requestUrl);
  qDebug() << "[AICopilot] Model:" << QString::fromStdString(m_config.model);

  // Synchronous request using event loop (matches LLMClient.cpp pattern)
  QEventLoop loop;
  QTimer timer;
  timer.setSingleShot(true);

  QNetworkReply *reply = m_networkManager->post(
      httpRequest, QByteArray::fromStdString(requestBody));

  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

  timer.start(120000); // 120 second timeout (large models need more time)
  loop.exec();

  qDebug() << "[AICopilot] Request finished, timer active:" << timer.isActive();

  std::string response;
  if (timer.isActive()) {
    timer.stop();
    if (reply->error() == QNetworkReply::NoError) {
      response = reply->readAll().toStdString();
      qDebug() << "[AICopilot] Got response, length:" << response.length();
    } else {
      reply->deleteLater();
      return "Network error: " + reply->errorString().toStdString();
    }
  } else {
    reply->abort();
    reply->deleteLater();
    qWarning() << "[AICopilot] Request timed out. Is Ollama running?";
    return "Request timed out. Is Ollama running?";
  }

  reply->deleteLater();

  // Parse Ollama response (supports /api/chat, /api/generate and
  // /v1/chat/completions)
  try {
    json jsonResponse = json::parse(response);

    // Check for /api/generate format
    if (jsonResponse.contains("response")) {
      return jsonResponse["response"].get<std::string>();
    }
    // Check for /api/chat format
    if (jsonResponse.contains("message") &&
        jsonResponse["message"].contains("content")) {
      return jsonResponse["message"]["content"].get<std::string>();
    }
    // Check for /v1/chat/completions format (OpenAI-compatible)
    if (jsonResponse.contains("choices") &&
        jsonResponse["choices"].is_array()) {
      auto &choices = jsonResponse["choices"];
      if (!choices.empty() && choices[0].contains("message")) {
        return choices[0]["message"]["content"].get<std::string>();
      }
    }
    // Check for error
    if (jsonResponse.contains("error")) {
      return "Error: " + jsonResponse["error"].get<std::string>();
    }
  } catch (const std::exception &e) {
    return "Failed to parse LLM response: " + std::string(e.what()) +
           "\nRaw: " + response;
  }

  return response;
}

json AICopilotAgent::ParseLLMResponse(const std::string &response) {
  json result;
  result["text"] = response;
  result["toolCalls"] = json::array();

  // Look for tool call blocks
  // Allow optional language hints after ```tool and tolerate Windows line
  // endings
  std::regex toolRegex("```tool[^\\n]*\\r?\\n([\\s\\S]*?)```",
                       std::regex::icase);
  std::smatch match;
  std::string remaining = response;

  while (std::regex_search(remaining, match, toolRegex)) {
    try {
      std::string block = match[1].str();
      // Trim leading/trailing whitespace to reduce parse failures
      block.erase(block.begin(), std::find_if(block.begin(), block.end(),
                                              [](unsigned char ch) {
                                                return !std::isspace(ch);
                                              }));
      block.erase(
          std::find_if(block.rbegin(), block.rend(),
                       [](unsigned char ch) { return !std::isspace(ch); })
              .base(),
          block.end());

      json toolCall = json::parse(block);
      result["toolCalls"].push_back(toolCall);
    } catch (...) {
      // Ignore parse errors
    }
    remaining = match.suffix();
  }

  return result;
}

json AICopilotAgent::ExecuteToolCall(const std::string &toolName,
                                     const json &params) {
  for (const auto &tool : m_tools) {
    if (tool.name == toolName) {
      try {
        return tool.execute(params);
      } catch (const std::exception &e) {
        return {{"error", e.what()}};
      }
    }
  }

  return {{"error", "Unknown tool: " + toolName}};
}

std::string AICopilotAgent::SendMessage(const std::string &userMessage) {
  // Add user message to history
  Message userMsg;
  userMsg.role = Message::Role::User;
  userMsg.content = userMessage;
  m_history.push_back(userMsg);

  // Build full prompt and call LLM
  std::string fullPrompt = BuildPromptWithContext();
  std::string response = CallLLM(fullPrompt);

  // Add assistant response to history
  Message assistantMsg;
  assistantMsg.role = Message::Role::Assistant;
  assistantMsg.content = response;
  m_history.push_back(assistantMsg);

  return response;
}

std::string
AICopilotAgent::ProcessAgenticRequest(const std::string &userMessage) {
  // Add user message
  Message userMsg;
  userMsg.role = Message::Role::User;
  userMsg.content = userMessage;
  m_history.push_back(userMsg);

  const int maxIterations = 5; // Prevent infinite loops
  std::string finalResponse;
  std::vector<std::string> actionsSummary;

  for (int i = 0; i < maxIterations; ++i) {
    // Build prompt and call LLM
    std::string fullPrompt = BuildPromptWithContext();
    std::string llmResponse = CallLLM(fullPrompt);

    // Parse response for tool calls
    json parsed = ParseLLMResponse(llmResponse);

    // Add assistant message
    Message assistantMsg;
    assistantMsg.role = Message::Role::Assistant;
    assistantMsg.content = llmResponse;
    assistantMsg.toolCalls = parsed["toolCalls"];
    m_history.push_back(assistantMsg);

    // Execute any tool calls
    auto &toolCalls = parsed["toolCalls"];
    if (toolCalls.empty()) {
      // No more tool calls, we're done - clean up the response
      finalResponse = CleanResponseForUser(llmResponse);
      break;
    }

    // Execute each tool call and add results to history
    for (const auto &call : toolCalls) {
      std::string toolName = call.value("tool", "");
      json params = call.value("params", json::object());

      json result = ExecuteToolCall(toolName, params);

      // Track what was done for user summary
      if (result.contains("message")) {
        actionsSummary.push_back(result["message"].get<std::string>());
      }

      // Add tool result as system message
      Message resultMsg;
      resultMsg.role = Message::Role::System;
      resultMsg.content = "Tool '" + toolName + "' result: " + result.dump();
      m_history.push_back(resultMsg);
    }

    finalResponse = CleanResponseForUser(llmResponse);
  }

  // If we executed tools, provide a clean summary
  if (!actionsSummary.empty()) {
    std::string summary;
    for (const auto &action : actionsSummary) {
      if (!summary.empty())
        summary += "\n";
      summary += action;
    }
    // Append any remaining cleaned response
    std::string cleaned = CleanResponseForUser(finalResponse);
    if (!cleaned.empty() && cleaned.find("```") == std::string::npos) {
      summary += "\n" + cleaned;
    }
    return summary;
  }

  return finalResponse;
}

std::string AICopilotAgent::CleanResponseForUser(const std::string &response) {
  // Remove tool call blocks from the response
  std::string cleaned = response;

  // Remove ```tool ... ``` blocks
  std::regex toolBlockRegex("```tool[^`]*```", std::regex::icase);
  cleaned = std::regex_replace(cleaned, toolBlockRegex, "");

  // Remove any JSON blocks that look like tool calls
  std::regex jsonToolRegex("```json[^`]*\"tool\"[^`]*```", std::regex::icase);
  cleaned = std::regex_replace(cleaned, jsonToolRegex, "");

  // Clean up excessive whitespace
  std::regex multiNewline("\n{3,}");
  cleaned = std::regex_replace(cleaned, multiNewline, "\n\n");

  // Trim leading/trailing whitespace
  auto start = cleaned.find_first_not_of(" \t\n\r");
  auto end = cleaned.find_last_not_of(" \t\n\r");
  if (start != std::string::npos && end != std::string::npos) {
    cleaned = cleaned.substr(start, end - start + 1);
  } else {
    cleaned.clear();
  }

  return cleaned;
}

} // namespace Aetherion::Editor
