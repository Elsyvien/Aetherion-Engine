#include "Aetherion/Editor/AICopilotAgent.h"
#include <iostream>
#include <regex>
#include <sstream>

#include <QByteArray>
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
      R"(You are an AI assistant for the Aetherion Game Engine. You help users create, modify, and manage game entities, scenes, and components.

You have access to tools that let you interact with the engine. When a user asks you to do something in the scene, use the appropriate tools.

IMPORTANT RULES:
1. Always use tools when the user asks to create, modify, or delete something
2. Explain what you're doing before and after using tools
3. If you're unsure, ask clarifying questions
4. Be concise but helpful
5. When creating entities, suggest appropriate components

AVAILABLE ENTITY TYPES:
- Cube, Sphere, Cylinder, Plane, Capsule (primitives)
- PointLight, DirectionalLight, SpotLight (lights)
- Camera
- Empty (empty entity for grouping)

AVAILABLE COMPONENTS:
- TransformComponent: position (x,y,z), rotation (x,y,z), scale (x,y,z)
- MeshRendererComponent: mesh, material
- LightComponent: color (r,g,b), intensity, range, type
- CameraComponent: fov, near, far
- RigidbodyComponent: mass, isKinematic, useGravity
- ColliderComponent: type (box, sphere, capsule), size
- AudioSourceComponent: clip, volume, loop
- SkeletonComponent: skeleton asset path
- AnimatorComponent: clips, speed, rootMotion
- AIBehaviorComponent: behaviorType, parameters

When using tools, output a JSON block with this format:
```tool
{
    "tool": "tool_name",
    "params": { ... }
}
```

After tool execution, you'll receive the result and can continue the conversation.)";
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
  ss << "\nAVAILABLE TOOLS:\n";

  for (const auto &tool : m_tools) {
    ss << "\n## " << tool.name << "\n";
    ss << "Description: " << tool.description << "\n";
    ss << "Parameters: " << tool.parameters.dump(2) << "\n";
  }

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

  const bool hasGenerate = requestUrl.size() >= 13 &&
                           requestUrl.rfind("/api/generate") ==
                               requestUrl.size() - 13;
  const bool hasChatApi = requestUrl.size() >= 9 &&
                          requestUrl.rfind("/api/chat") ==
                              requestUrl.size() - 9;
  const bool hasChatCompletions = requestUrl.size() >= 18 &&
                                  requestUrl.rfind("/chat/completions") ==
                                      requestUrl.size() - 18;

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

  // Synchronous request using event loop (matches LLMClient.cpp pattern)
  QEventLoop loop;
  QTimer timer;
  timer.setSingleShot(true);

  QNetworkReply *reply = m_networkManager->post(
      httpRequest, QByteArray::fromStdString(requestBody));

  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

  timer.start(30000); // 30 second timeout
  loop.exec();

  std::string response;
  if (timer.isActive()) {
    timer.stop();
    if (reply->error() == QNetworkReply::NoError) {
      response = reply->readAll().toStdString();
    } else {
      reply->deleteLater();
      return "Network error: " + reply->errorString().toStdString();
    }
  } else {
    reply->abort();
    reply->deleteLater();
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
  std::regex toolRegex("```tool\\s*\\n([\\s\\S]*?)\\n```");
  std::smatch match;
  std::string remaining = response;

  while (std::regex_search(remaining, match, toolRegex)) {
    try {
      json toolCall = json::parse(match[1].str());
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
      // No more tool calls, we're done
      finalResponse = llmResponse;
      break;
    }

    // Execute each tool call and add results to history
    for (const auto &call : toolCalls) {
      std::string toolName = call.value("tool", "");
      json params = call.value("params", json::object());

      json result = ExecuteToolCall(toolName, params);

      // Add tool result as system message
      Message resultMsg;
      resultMsg.role = Message::Role::System;
      resultMsg.content = "Tool '" + toolName + "' result: " + result.dump();
      m_history.push_back(resultMsg);
    }

    finalResponse = llmResponse;
  }

  return finalResponse;
}

} // namespace Aetherion::Editor
