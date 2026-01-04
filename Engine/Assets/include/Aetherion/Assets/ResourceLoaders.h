#pragma once

#include "ResourceManager.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <vector>

namespace Aetherion::Assets {

// =============================================================================
// Common Resource Types
// =============================================================================

/// @brief Raw binary data resource
struct BinaryData {
  std::vector<uint8_t> data;
  
  [[nodiscard]] size_t Size() const noexcept { return data.size(); }
  [[nodiscard]] const uint8_t* Data() const noexcept { return data.data(); }
  [[nodiscard]] uint8_t* Data() noexcept { return data.data(); }
};

/// @brief Text file resource
struct TextData {
  std::string content;
  
  [[nodiscard]] size_t Size() const noexcept { return content.size(); }
  [[nodiscard]] const std::string& Content() const noexcept { return content; }
  
  /// @brief Get lines of text
  [[nodiscard]] std::vector<std::string> GetLines() const {
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
      lines.push_back(line);
    }
    return lines;
  }
};

/// @brief JSON document resource
struct JsonData {
  std::string content;
  // Note: Actual parsing would use a JSON library like nlohmann/json
  
  [[nodiscard]] const std::string& Raw() const noexcept { return content; }
};

/// @brief Image data (raw pixels, not GPU texture)
struct ImageData {
  std::vector<uint8_t> pixels;
  uint32_t width{0};
  uint32_t height{0};
  uint32_t channels{4}; // RGBA by default
  bool hdr{false};
  
  [[nodiscard]] size_t Size() const noexcept { return pixels.size(); }
  [[nodiscard]] size_t PixelCount() const noexcept { return width * height; }
  [[nodiscard]] size_t BytesPerPixel() const noexcept { return hdr ? channels * 4 : channels; }
};

/// @brief Shader source code
struct ShaderSource {
  std::string vertexSource;
  std::string fragmentSource;
  std::string geometrySource;
  std::string computeSource;
  std::string path;
  
  [[nodiscard]] bool HasVertex() const { return !vertexSource.empty(); }
  [[nodiscard]] bool HasFragment() const { return !fragmentSource.empty(); }
  [[nodiscard]] bool HasGeometry() const { return !geometrySource.empty(); }
  [[nodiscard]] bool HasCompute() const { return !computeSource.empty(); }
};

/// @brief Audio sample data
struct AudioData {
  std::vector<float> samples; // Interleaved samples
  uint32_t sampleRate{44100};
  uint32_t channels{2};
  double duration{0.0}; // In seconds
  
  [[nodiscard]] size_t SampleCount() const { return samples.size() / channels; }
  [[nodiscard]] size_t FrameCount() const { return SampleCount(); }
};

// =============================================================================
// Resource Loaders
// =============================================================================

/// @brief Loader for binary files
class BinaryLoader : public ResourceLoader<BinaryData> {
public:
  [[nodiscard]] std::vector<std::string> GetSupportedExtensions() const override {
    return {".bin", ".dat", ".raw", ".bytes"};
  }
  
protected:
  [[nodiscard]] std::shared_ptr<BinaryData> LoadTyped(
    const std::filesystem::path& path,
    std::string& outError) override {
    
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      outError = "Failed to open file: " + path.string();
      return nullptr;
    }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    auto data = std::make_shared<BinaryData>();
    data->data.resize(static_cast<size_t>(size));
    
    if (!file.read(reinterpret_cast<char*>(data->data.data()), size)) {
      outError = "Failed to read file: " + path.string();
      return nullptr;
    }
    
    return data;
  }
  
  [[nodiscard]] size_t EstimateSizeTyped(const std::shared_ptr<BinaryData>& resource) const override {
    return sizeof(BinaryData) + resource->data.size();
  }
};

/// @brief Loader for text files
class TextLoader : public ResourceLoader<TextData> {
public:
  [[nodiscard]] std::vector<std::string> GetSupportedExtensions() const override {
    return {".txt", ".md", ".log", ".ini", ".cfg", ".yaml", ".yml"};
  }
  
protected:
  [[nodiscard]] std::shared_ptr<TextData> LoadTyped(
    const std::filesystem::path& path,
    std::string& outError) override {
    
    std::ifstream file(path);
    if (!file.is_open()) {
      outError = "Failed to open file: " + path.string();
      return nullptr;
    }
    
    auto data = std::make_shared<TextData>();
    std::stringstream buffer;
    buffer << file.rdbuf();
    data->content = buffer.str();
    
    return data;
  }
  
  [[nodiscard]] size_t EstimateSizeTyped(const std::shared_ptr<TextData>& resource) const override {
    return sizeof(TextData) + resource->content.size();
  }
};

/// @brief Loader for JSON files
class JsonLoader : public ResourceLoader<JsonData> {
public:
  [[nodiscard]] std::vector<std::string> GetSupportedExtensions() const override {
    return {".json"};
  }
  
protected:
  [[nodiscard]] std::shared_ptr<JsonData> LoadTyped(
    const std::filesystem::path& path,
    std::string& outError) override {
    
    std::ifstream file(path);
    if (!file.is_open()) {
      outError = "Failed to open file: " + path.string();
      return nullptr;
    }
    
    auto data = std::make_shared<JsonData>();
    std::stringstream buffer;
    buffer << file.rdbuf();
    data->content = buffer.str();
    
    return data;
  }
  
  [[nodiscard]] size_t EstimateSizeTyped(const std::shared_ptr<JsonData>& resource) const override {
    return sizeof(JsonData) + resource->content.size();
  }
};

/// @brief Loader for GLSL shader source files
class ShaderSourceLoader : public ResourceLoader<ShaderSource> {
public:
  [[nodiscard]] std::vector<std::string> GetSupportedExtensions() const override {
    return {".glsl", ".vert", ".frag", ".geom", ".comp", ".shader"};
  }
  
protected:
  [[nodiscard]] std::shared_ptr<ShaderSource> LoadTyped(
    const std::filesystem::path& path,
    std::string& outError) override {
    
    std::ifstream file(path);
    if (!file.is_open()) {
      outError = "Failed to open file: " + path.string();
      return nullptr;
    }
    
    auto data = std::make_shared<ShaderSource>();
    data->path = path.string();
    
    std::string extension = path.extension().string();
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Single-type shader files
    if (extension == ".vert") {
      data->vertexSource = content;
    } else if (extension == ".frag") {
      data->fragmentSource = content;
    } else if (extension == ".geom") {
      data->geometrySource = content;
    } else if (extension == ".comp") {
      data->computeSource = content;
    } else {
      // Combined shader file - parse sections
      ParseCombinedShader(content, *data);
    }
    
    return data;
  }
  
private:
  void ParseCombinedShader(const std::string& content, ShaderSource& shader) {
    // Parse #pragma section markers like:
    // #pragma vertex
    // #pragma fragment
    // #pragma geometry
    // #pragma compute
    
    std::istringstream stream(content);
    std::string line;
    std::string currentSection;
    std::ostringstream sectionContent;
    
    while (std::getline(stream, line)) {
      if (line.find("#pragma vertex") != std::string::npos) {
        SaveSection(currentSection, sectionContent.str(), shader);
        currentSection = "vertex";
        sectionContent.str("");
        sectionContent.clear();
      } else if (line.find("#pragma fragment") != std::string::npos) {
        SaveSection(currentSection, sectionContent.str(), shader);
        currentSection = "fragment";
        sectionContent.str("");
        sectionContent.clear();
      } else if (line.find("#pragma geometry") != std::string::npos) {
        SaveSection(currentSection, sectionContent.str(), shader);
        currentSection = "geometry";
        sectionContent.str("");
        sectionContent.clear();
      } else if (line.find("#pragma compute") != std::string::npos) {
        SaveSection(currentSection, sectionContent.str(), shader);
        currentSection = "compute";
        sectionContent.str("");
        sectionContent.clear();
      } else {
        sectionContent << line << "\n";
      }
    }
    
    SaveSection(currentSection, sectionContent.str(), shader);
  }
  
  void SaveSection(const std::string& section, const std::string& content, 
                   ShaderSource& shader) {
    if (section == "vertex") shader.vertexSource = content;
    else if (section == "fragment") shader.fragmentSource = content;
    else if (section == "geometry") shader.geometrySource = content;
    else if (section == "compute") shader.computeSource = content;
  }
};

/// @brief Factory for creating a ResourceManager with standard loaders
inline std::unique_ptr<ResourceManager> CreateStandardResourceManager(size_t numWorkers = 0) {
  auto manager = std::make_unique<ResourceManager>();
  manager->Initialize(numWorkers);
  
  // Register standard loaders
  manager->RegisterLoader<BinaryData>(std::make_unique<BinaryLoader>());
  manager->RegisterLoader<TextData>(std::make_unique<TextLoader>());
  manager->RegisterLoader<JsonData>(std::make_unique<JsonLoader>());
  manager->RegisterLoader<ShaderSource>(std::make_unique<ShaderSourceLoader>());
  
  return manager;
}

} // namespace Aetherion::Assets
