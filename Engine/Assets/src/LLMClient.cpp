#include "Aetherion/Assets/LLMClient.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>

#include <chrono>
#include <cstring>

namespace Aetherion::Assets {

// =============================================================================
// LLMConfig
// =============================================================================

std::string LLMConfig::GetDefaultEndpoint(LLMProvider provider) {
    switch (provider) {
        case LLMProvider::OpenAI:
            return "https://api.openai.com/v1";
        case LLMProvider::Anthropic:
            return "https://api.anthropic.com/v1";
        case LLMProvider::Google:
            return "https://generativelanguage.googleapis.com/v1beta";
        case LLMProvider::Replicate:
            return "https://api.replicate.com/v1";
        case LLMProvider::StabilityAI:
            return "https://api.stability.ai/v1";
        case LLMProvider::LocalOllama:
            return "http://localhost:11434/v1";
        case LLMProvider::Custom:
        default:
            return "";
    }
}

std::string LLMConfig::GetDefaultModel(LLMProvider provider) {
    switch (provider) {
        case LLMProvider::OpenAI:
            return "gpt-4o";
        case LLMProvider::Anthropic:
            return "claude-sonnet-4-20250514";
        case LLMProvider::Google:
            return "gemini-1.5-pro";
        case LLMProvider::LocalOllama:
            return "llama3.1";
        case LLMProvider::Custom:
        case LLMProvider::Replicate:
        case LLMProvider::StabilityAI:
        default:
            return "";
    }
}

std::string LLMConfig::GetDefaultImageModel(LLMProvider provider) {
    switch (provider) {
        case LLMProvider::OpenAI:
            return "dall-e-3";
        case LLMProvider::StabilityAI:
            return "stable-diffusion-xl-1024-v1-0";
        case LLMProvider::Replicate:
            return "stability-ai/sdxl";
        default:
            return "";
    }
}

// =============================================================================
// LLMClientFactory
// =============================================================================

std::unique_ptr<ILLMClient> LLMClientFactory::Create(LLMProvider provider) {
    LLMConfig config;
    config.provider = provider;
    config.endpoint = LLMConfig::GetDefaultEndpoint(provider);
    config.model = LLMConfig::GetDefaultModel(provider);
    config.imageModel = LLMConfig::GetDefaultImageModel(provider);
    return Create(config);
}

std::unique_ptr<ILLMClient> LLMClientFactory::Create(const LLMConfig& config) {
    std::unique_ptr<ILLMClient> client;
    
    switch (config.provider) {
        case LLMProvider::OpenAI:
        case LLMProvider::LocalOllama:  // Ollama is OpenAI-compatible
        case LLMProvider::Custom:
            client = std::make_unique<OpenAIClient>();
            break;
        case LLMProvider::Anthropic:
            client = std::make_unique<AnthropicClient>();
            break;
        case LLMProvider::StabilityAI:
            client = std::make_unique<StabilityAIClient>();
            break;
        default:
            // Fall back to OpenAI-compatible client
            client = std::make_unique<OpenAIClient>();
            break;
    }
    
    if (client) {
        client->Initialize(config);
    }
    
    return client;
}

std::vector<std::pair<LLMProvider, std::string>> LLMClientFactory::GetAvailableProviders() {
    return {
        {LLMProvider::OpenAI, "OpenAI (GPT-4, DALL-E)"},
        {LLMProvider::Anthropic, "Anthropic (Claude)"},
        {LLMProvider::StabilityAI, "Stability AI (Stable Diffusion)"},
        {LLMProvider::LocalOllama, "Local Ollama"},
        {LLMProvider::Custom, "Custom Endpoint"}
    };
}

// =============================================================================
// OpenAIClient
// =============================================================================

bool OpenAIClient::Initialize(const LLMConfig& config) {
    m_config = config;
    
    // Set defaults if not provided
    if (m_config.endpoint.empty()) {
        m_config.endpoint = LLMConfig::GetDefaultEndpoint(m_config.provider);
    }
    if (m_config.model.empty()) {
        m_config.model = LLMConfig::GetDefaultModel(m_config.provider);
    }
    if (m_config.imageModel.empty()) {
        m_config.imageModel = LLMConfig::GetDefaultImageModel(m_config.provider);
    }
    
    m_initialized = true;
    return true;
}

bool OpenAIClient::IsReady() const {
    if (!m_initialized) {
        return false;
    }
    if (m_config.provider == LLMProvider::LocalOllama) {
        return true;
    }
    return !m_config.apiKey.empty();
}

bool OpenAIClient::TestConnection() {
    if (!IsReady()) {
        return false;
    }
    
    // Try a simple models list request
    std::unordered_map<std::string, std::string> headers;
    if (!m_config.apiKey.empty()) {
        headers["Authorization"] = "Bearer " + m_config.apiKey;
    }
    headers["Content-Type"] = "application/json";
    
    auto response = DoHttpGet(m_config.endpoint + "/models", headers);
    return response.statusCode == 200;
}

OpenAIClient::HttpResponse OpenAIClient::DoHttpPost(
    const std::string& url,
    const std::string& body,
    const std::unordered_map<std::string, std::string>& headers) {
    
    HttpResponse result;
    
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(QString::fromStdString(url)));
    
    for (const auto& [key, value] : headers) {
        request.setRawHeader(QByteArray::fromStdString(key),
                            QByteArray::fromStdString(value));
    }
    
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    
    QNetworkReply* reply = manager.post(request, QByteArray::fromStdString(body));
    
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timer.start(m_config.timeoutMs);
    loop.exec();
    
    if (timer.isActive()) {
        timer.stop();
        result.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.body = reply->readAll().toStdString();
        
        if (reply->error() != QNetworkReply::NoError) {
            result.error = reply->errorString().toStdString();
        }
    } else {
        result.error = "Request timed out";
        reply->abort();
    }
    
    reply->deleteLater();
    return result;
}

OpenAIClient::HttpResponse OpenAIClient::DoHttpGet(
    const std::string& url,
    const std::unordered_map<std::string, std::string>& headers) {
    
    HttpResponse result;
    
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(QString::fromStdString(url)));
    
    for (const auto& [key, value] : headers) {
        request.setRawHeader(QByteArray::fromStdString(key),
                            QByteArray::fromStdString(value));
    }
    
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    
    QNetworkReply* reply = manager.get(request);
    
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timer.start(m_config.timeoutMs);
    loop.exec();
    
    if (timer.isActive()) {
        timer.stop();
        result.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.body = reply->readAll().toStdString();
        
        if (reply->error() != QNetworkReply::NoError) {
            result.error = reply->errorString().toStdString();
        }
    } else {
        result.error = "Request timed out";
        reply->abort();
    }
    
    reply->deleteLater();
    return result;
}

std::vector<uint8_t> OpenAIClient::DownloadImage(const std::string& url) {
    std::vector<uint8_t> result;
    
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(QString::fromStdString(url)));
    
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    
    QNetworkReply* reply = manager.get(request);
    
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timer.start(m_config.timeoutMs);
    loop.exec();
    
    if (timer.isActive()) {
        timer.stop();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            result.resize(static_cast<size_t>(data.size()));
            std::memcpy(result.data(), data.constData(), result.size());
        }
    } else {
        reply->abort();
    }
    
    reply->deleteLater();
    return result;
}

LLMTextResponse OpenAIClient::GenerateText(const LLMTextRequest& request) {
    LLMTextResponse result;
    
    if (!IsReady()) {
        result.errorMessage = "Client not initialized or API key missing";
        return result;
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    // Build request JSON
    QJsonObject json;
    json["model"] = QString::fromStdString(m_config.model);
    json["max_tokens"] = request.maxTokens;
    json["temperature"] = static_cast<double>(request.temperature);
    
    QJsonArray messages;
    
    if (!request.systemPrompt.empty()) {
        QJsonObject sysMsg;
        sysMsg["role"] = "system";
        sysMsg["content"] = QString::fromStdString(request.systemPrompt);
        messages.append(sysMsg);
    }
    
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = QString::fromStdString(request.userPrompt);
    messages.append(userMsg);
    
    json["messages"] = messages;
    
    if (!request.stopSequences.empty()) {
        QJsonArray stops;
        for (const auto& s : request.stopSequences) {
            stops.append(QString::fromStdString(s));
        }
        json["stop"] = stops;
    }
    
    std::string body = QJsonDocument(json).toJson(QJsonDocument::Compact).toStdString();
    
    std::unordered_map<std::string, std::string> headers;
    if (!m_config.apiKey.empty()) {
        headers["Authorization"] = "Bearer " + m_config.apiKey;
    }
    headers["Content-Type"] = "application/json";
    
    auto response = DoHttpPost(m_config.endpoint + "/chat/completions", body, headers);
    
    auto endTime = std::chrono::steady_clock::now();
    result.latencyMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());
    result.statusCode = response.statusCode;
    
    if (response.statusCode == 200) {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
        QJsonObject obj = doc.object();
        
        if (obj.contains("choices") && obj["choices"].isArray()) {
            QJsonArray choices = obj["choices"].toArray();
            if (!choices.isEmpty()) {
                QJsonObject choice = choices[0].toObject();
                if (choice.contains("message")) {
                    QJsonObject message = choice["message"].toObject();
                    result.content = message["content"].toString().toStdString();
                    result.success = true;
                }
            }
        }
        
        if (obj.contains("usage")) {
            QJsonObject usage = obj["usage"].toObject();
            result.promptTokens = static_cast<std::uint64_t>(usage["prompt_tokens"].toInt());
            result.completionTokens = static_cast<std::uint64_t>(usage["completion_tokens"].toInt());
        }
    } else {
        result.errorMessage = response.error.empty() ? response.body : response.error;
    }
    
    return result;
}

LLMImageResponse OpenAIClient::GenerateImage(const LLMImageRequest& request) {
    LLMImageResponse result;
    
    if (!IsReady()) {
        result.errorMessage = "Client not initialized or API key missing";
        return result;
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    // Build request JSON
    QJsonObject json;
    json["model"] = QString::fromStdString(m_config.imageModel);
    json["prompt"] = QString::fromStdString(request.prompt);
    json["n"] = request.numImages;
    json["size"] = QString("%1x%2").arg(request.width).arg(request.height);
    json["response_format"] = "url";  // or "b64_json" for base64
    
    if (!request.quality.empty()) {
        json["quality"] = QString::fromStdString(request.quality);
    }
    if (!request.style.empty()) {
        json["style"] = QString::fromStdString(request.style);
    }
    
    std::string body = QJsonDocument(json).toJson(QJsonDocument::Compact).toStdString();
    
    std::unordered_map<std::string, std::string> headers;
    if (!m_config.apiKey.empty()) {
        headers["Authorization"] = "Bearer " + m_config.apiKey;
    }
    headers["Content-Type"] = "application/json";
    
    auto response = DoHttpPost(m_config.endpoint + "/images/generations", body, headers);
    
    auto endTime = std::chrono::steady_clock::now();
    result.latencyMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());
    result.statusCode = response.statusCode;
    
    if (response.statusCode == 200) {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
        QJsonObject obj = doc.object();
        
        if (obj.contains("data") && obj["data"].isArray()) {
            QJsonArray data = obj["data"].toArray();
            for (const auto& item : data) {
                QJsonObject imgObj = item.toObject();
                
                if (imgObj.contains("url")) {
                    std::string url = imgObj["url"].toString().toStdString();
                    result.imageUrls.push_back(url);
                    
                    // Download the image
                    auto imageData = DownloadImage(url);
                    if (!imageData.empty()) {
                        result.images.push_back(std::move(imageData));
                    }
                }
                
                if (imgObj.contains("b64_json")) {
                    QByteArray base64 = imgObj["b64_json"].toString().toLatin1();
                    QByteArray decoded = QByteArray::fromBase64(base64);
                    std::vector<uint8_t> imageData(static_cast<size_t>(decoded.size()));
                    std::memcpy(imageData.data(), decoded.constData(), imageData.size());
                    result.images.push_back(std::move(imageData));
                }
                
                if (imgObj.contains("revised_prompt")) {
                    result.revisedPrompt = imgObj["revised_prompt"].toString().toStdString();
                }
            }
            result.success = !result.images.empty() || !result.imageUrls.empty();
        }
    } else {
        result.errorMessage = response.error.empty() ? response.body : response.error;
    }
    
    return result;
}

LLMTextResponse OpenAIClient::GenerateJSON(
    const std::string& systemPrompt,
    const std::string& userPrompt,
    const std::string& jsonSchema) {
    
    LLMTextRequest request;
    request.systemPrompt = systemPrompt;
    if (!jsonSchema.empty()) {
        request.systemPrompt += "\n\nRespond ONLY with valid JSON matching this schema:\n" + jsonSchema;
    }
    request.userPrompt = userPrompt;
    request.temperature = 0.3f;  // Lower temperature for structured output
    
    return GenerateText(request);
}

// =============================================================================
// AnthropicClient
// =============================================================================

bool AnthropicClient::Initialize(const LLMConfig& config) {
    m_config = config;
    
    if (m_config.endpoint.empty()) {
        m_config.endpoint = LLMConfig::GetDefaultEndpoint(LLMProvider::Anthropic);
    }
    if (m_config.model.empty()) {
        m_config.model = LLMConfig::GetDefaultModel(LLMProvider::Anthropic);
    }
    
    m_initialized = true;
    return true;
}

bool AnthropicClient::IsReady() const {
    return m_initialized && !m_config.apiKey.empty();
}

bool AnthropicClient::TestConnection() {
    // Anthropic doesn't have a simple health check endpoint
    // Just verify we have credentials
    return IsReady();
}

AnthropicClient::HttpResponse AnthropicClient::DoHttpPost(
    const std::string& url,
    const std::string& body,
    const std::unordered_map<std::string, std::string>& headers) {
    
    HttpResponse result;
    
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(QString::fromStdString(url)));
    
    for (const auto& [key, value] : headers) {
        request.setRawHeader(QByteArray::fromStdString(key),
                            QByteArray::fromStdString(value));
    }
    
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    
    QNetworkReply* reply = manager.post(request, QByteArray::fromStdString(body));
    
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timer.start(m_config.timeoutMs);
    loop.exec();
    
    if (timer.isActive()) {
        timer.stop();
        result.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.body = reply->readAll().toStdString();
        
        if (reply->error() != QNetworkReply::NoError) {
            result.error = reply->errorString().toStdString();
        }
    } else {
        result.error = "Request timed out";
        reply->abort();
    }
    
    reply->deleteLater();
    return result;
}

LLMTextResponse AnthropicClient::GenerateText(const LLMTextRequest& request) {
    LLMTextResponse result;
    
    if (!IsReady()) {
        result.errorMessage = "Client not initialized or API key missing";
        return result;
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    // Build request JSON (Anthropic format)
    QJsonObject json;
    json["model"] = QString::fromStdString(m_config.model);
    json["max_tokens"] = request.maxTokens;
    
    if (!request.systemPrompt.empty()) {
        json["system"] = QString::fromStdString(request.systemPrompt);
    }
    
    QJsonArray messages;
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = QString::fromStdString(request.userPrompt);
    messages.append(userMsg);
    json["messages"] = messages;
    
    std::string body = QJsonDocument(json).toJson(QJsonDocument::Compact).toStdString();
    
    std::unordered_map<std::string, std::string> headers;
    headers["x-api-key"] = m_config.apiKey;
    headers["anthropic-version"] = "2023-06-01";
    headers["Content-Type"] = "application/json";
    
    auto response = DoHttpPost(m_config.endpoint + "/messages", body, headers);
    
    auto endTime = std::chrono::steady_clock::now();
    result.latencyMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());
    result.statusCode = response.statusCode;
    
    if (response.statusCode == 200) {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
        QJsonObject obj = doc.object();
        
        if (obj.contains("content") && obj["content"].isArray()) {
            QJsonArray content = obj["content"].toArray();
            for (const auto& item : content) {
                QJsonObject block = item.toObject();
                if (block["type"].toString() == "text") {
                    result.content += block["text"].toString().toStdString();
                }
            }
            result.success = true;
        }
        
        if (obj.contains("usage")) {
            QJsonObject usage = obj["usage"].toObject();
            result.promptTokens = static_cast<std::uint64_t>(usage["input_tokens"].toInt());
            result.completionTokens = static_cast<std::uint64_t>(usage["output_tokens"].toInt());
        }
    } else {
        result.errorMessage = response.error.empty() ? response.body : response.error;
    }
    
    return result;
}

LLMImageResponse AnthropicClient::GenerateImage(const LLMImageRequest& /*request*/) {
    LLMImageResponse result;
    result.errorMessage = "Anthropic does not support image generation. Use OpenAI or Stability AI.";
    return result;
}

LLMTextResponse AnthropicClient::GenerateJSON(
    const std::string& systemPrompt,
    const std::string& userPrompt,
    const std::string& jsonSchema) {
    
    LLMTextRequest request;
    request.systemPrompt = systemPrompt;
    if (!jsonSchema.empty()) {
        request.systemPrompt += "\n\nRespond ONLY with valid JSON matching this schema:\n" + jsonSchema;
    }
    request.userPrompt = userPrompt;
    request.temperature = 0.3f;
    
    return GenerateText(request);
}

// =============================================================================
// StabilityAIClient
// =============================================================================

bool StabilityAIClient::Initialize(const LLMConfig& config) {
    m_config = config;
    
    if (m_config.endpoint.empty()) {
        m_config.endpoint = LLMConfig::GetDefaultEndpoint(LLMProvider::StabilityAI);
    }
    if (m_config.imageModel.empty()) {
        m_config.imageModel = LLMConfig::GetDefaultImageModel(LLMProvider::StabilityAI);
    }
    
    m_initialized = true;
    return true;
}

bool StabilityAIClient::IsReady() const {
    return m_initialized && !m_config.apiKey.empty();
}

bool StabilityAIClient::TestConnection() {
    return IsReady();
}

StabilityAIClient::HttpResponse StabilityAIClient::DoHttpPost(
    const std::string& url,
    const std::string& body,
    const std::unordered_map<std::string, std::string>& headers) {
    
    HttpResponse result;
    
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(QString::fromStdString(url)));
    
    for (const auto& [key, value] : headers) {
        request.setRawHeader(QByteArray::fromStdString(key),
                            QByteArray::fromStdString(value));
    }
    
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    
    QNetworkReply* reply = manager.post(request, QByteArray::fromStdString(body));
    
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timer.start(m_config.timeoutMs);
    loop.exec();
    
    if (timer.isActive()) {
        timer.stop();
        result.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.body = reply->readAll().toStdString();
        
        if (reply->error() != QNetworkReply::NoError) {
            result.error = reply->errorString().toStdString();
        }
    } else {
        result.error = "Request timed out";
        reply->abort();
    }
    
    reply->deleteLater();
    return result;
}

LLMTextResponse StabilityAIClient::GenerateText(const LLMTextRequest& /*request*/) {
    LLMTextResponse result;
    result.errorMessage = "Stability AI is for image generation only. Use OpenAI or Anthropic for text.";
    return result;
}

LLMImageResponse StabilityAIClient::GenerateImage(const LLMImageRequest& request) {
    LLMImageResponse result;
    
    if (!IsReady()) {
        result.errorMessage = "Client not initialized or API key missing";
        return result;
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    // Build request JSON for Stability AI
    QJsonObject json;
    
    QJsonArray textPrompts;
    QJsonObject prompt;
    prompt["text"] = QString::fromStdString(request.prompt);
    prompt["weight"] = 1.0;
    textPrompts.append(prompt);
    json["text_prompts"] = textPrompts;
    
    json["cfg_scale"] = 7;
    json["height"] = request.height;
    json["width"] = request.width;
    json["samples"] = request.numImages;
    json["steps"] = 30;
    
    std::string body = QJsonDocument(json).toJson(QJsonDocument::Compact).toStdString();
    
    std::unordered_map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer " + m_config.apiKey;
    headers["Content-Type"] = "application/json";
    headers["Accept"] = "application/json";
    
    std::string url = m_config.endpoint + "/generation/" + m_config.imageModel + "/text-to-image";
    auto response = DoHttpPost(url, body, headers);
    
    auto endTime = std::chrono::steady_clock::now();
    result.latencyMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());
    result.statusCode = response.statusCode;
    
    if (response.statusCode == 200) {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body));
        QJsonObject obj = doc.object();
        
        if (obj.contains("artifacts") && obj["artifacts"].isArray()) {
            QJsonArray artifacts = obj["artifacts"].toArray();
            for (const auto& item : artifacts) {
                QJsonObject artifact = item.toObject();
                
                if (artifact.contains("base64")) {
                    QByteArray base64 = artifact["base64"].toString().toLatin1();
                    QByteArray decoded = QByteArray::fromBase64(base64);
                    std::vector<uint8_t> imageData(static_cast<size_t>(decoded.size()));
                    std::memcpy(imageData.data(), decoded.constData(), imageData.size());
                    result.images.push_back(std::move(imageData));
                }
            }
            result.success = !result.images.empty();
        }
    } else {
        result.errorMessage = response.error.empty() ? response.body : response.error;
    }
    
    return result;
}

LLMTextResponse StabilityAIClient::GenerateJSON(
    const std::string& /*systemPrompt*/,
    const std::string& /*userPrompt*/,
    const std::string& /*jsonSchema*/) {
    
    LLMTextResponse result;
    result.errorMessage = "Stability AI does not support text generation";
    return result;
}

} // namespace Aetherion::Assets
