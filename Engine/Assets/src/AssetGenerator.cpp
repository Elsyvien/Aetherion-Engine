#include "Aetherion/Assets/AssetGenerator.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {

std::uint64_t GetCurrentTimeMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

std::string ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool ContainsWord(const std::string& text, const std::string& word) {
    return ToLower(text).find(ToLower(word)) != std::string::npos;
}

// Simple Perlin-like noise function
float Noise2D(float x, float y) {
    int xi = static_cast<int>(std::floor(x)) & 255;
    int yi = static_cast<int>(std::floor(y)) & 255;
    float xf = x - std::floor(x);
    float yf = y - std::floor(y);
    
    // Use a simple hash
    auto hash = [](int x, int y) {
        int n = x + y * 57;
        n = (n << 13) ^ n;
        return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
    };
    
    // Smooth interpolation
    auto fade = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); };
    auto lerp = [](float a, float b, float t) { return a + t * (b - a); };
    
    float u = fade(xf);
    float v = fade(yf);
    
    return lerp(
        lerp(hash(xi, yi), hash(xi + 1, yi), u),
        lerp(hash(xi, yi + 1), hash(xi + 1, yi + 1), u),
        v
    );
}

float FractalNoise(float x, float y, int octaves = 4) {
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    
    for (int i = 0; i < octaves; ++i) {
        value += amplitude * Noise2D(x * frequency, y * frequency);
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    
    return value;
}

void WritePPM(const std::filesystem::path& path,
              const std::vector<uint8_t>& pixels,
              int width, int height) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return;
    
    file << "P6\n" << width << " " << height << "\n255\n";
    
    // PPM is RGB, not RGBA
    for (size_t i = 0; i < pixels.size(); i += 4) {
        file.put(static_cast<char>(pixels[i]));
        file.put(static_cast<char>(pixels[i + 1]));
        file.put(static_cast<char>(pixels[i + 2]));
    }
}

// Write a simple BMP file
void WriteBMP(const std::filesystem::path& path,
              const std::vector<uint8_t>& pixels,
              int width, int height) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return;

    const int rowSize = ((width * 3 + 3) / 4) * 4;  // Row padding to 4 bytes
    const int dataSize = rowSize * height;
    const int fileSize = 54 + dataSize;

    // BMP Header
    uint8_t header[54] = {
        'B', 'M',                               // Magic
        0, 0, 0, 0,                             // File size (filled below)
        0, 0, 0, 0,                             // Reserved
        54, 0, 0, 0,                            // Offset to pixel data
        40, 0, 0, 0,                            // DIB header size
        0, 0, 0, 0,                             // Width (filled below)
        0, 0, 0, 0,                             // Height (filled below)
        1, 0,                                   // Color planes
        24, 0,                                  // Bits per pixel
        0, 0, 0, 0,                             // Compression (none)
        0, 0, 0, 0,                             // Image size
        0, 0, 0, 0,                             // X pixels per meter
        0, 0, 0, 0,                             // Y pixels per meter
        0, 0, 0, 0,                             // Colors in palette
        0, 0, 0, 0                              // Important colors
    };

    // Fill in values
    std::memcpy(&header[2], &fileSize, 4);
    std::memcpy(&header[18], &width, 4);
    std::memcpy(&header[22], &height, 4);
    std::memcpy(&header[34], &dataSize, 4);

    file.write(reinterpret_cast<char*>(header), 54);

    // Write pixel data (BMP is BGR, bottom-to-top)
    std::vector<uint8_t> row(static_cast<size_t>(rowSize), 0);
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            size_t srcIdx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
            size_t dstIdx = static_cast<size_t>(x) * 3;
            row[dstIdx + 0] = pixels[srcIdx + 2];  // B
            row[dstIdx + 1] = pixels[srcIdx + 1];  // G
            row[dstIdx + 2] = pixels[srcIdx + 0];  // R
        }
        file.write(reinterpret_cast<char*>(row.data()), rowSize);
    }
}

void WriteOBJ(const std::filesystem::path& path,
              const std::vector<std::array<float, 3>>& vertices,
              const std::vector<std::array<int, 3>>& faces) {
    std::ofstream file(path);
    if (!file.is_open()) return;
    
    file << "# Generated by Aetherion AssetGenerator\n\n";
    
    for (const auto& v : vertices) {
        file << "v " << v[0] << " " << v[1] << " " << v[2] << "\n";
    }
    
    file << "\n";
    
    for (const auto& f : faces) {
        file << "f " << (f[0] + 1) << " " << (f[1] + 1) << " " << (f[2] + 1) << "\n";
    }
}

void WriteWAV(const std::filesystem::path& path,
              const std::vector<int16_t>& samples,
              int sampleRate = 44100) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return;
    
    const int numChannels = 1;
    const int bitsPerSample = 16;
    const int byteRate = sampleRate * numChannels * bitsPerSample / 8;
    const int blockAlign = numChannels * bitsPerSample / 8;
    const int dataSize = static_cast<int>(samples.size()) * 2;
    const int chunkSize = 36 + dataSize;
    
    // RIFF header
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&chunkSize), 4);
    file.write("WAVE", 4);
    
    // fmt subchunk
    file.write("fmt ", 4);
    int fmtSize = 16;
    file.write(reinterpret_cast<const char*>(&fmtSize), 4);
    int16_t audioFormat = 1;  // PCM
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    int16_t channels = static_cast<int16_t>(numChannels);
    file.write(reinterpret_cast<const char*>(&channels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    int16_t blockAlignShort = static_cast<int16_t>(blockAlign);
    file.write(reinterpret_cast<const char*>(&blockAlignShort), 2);
    int16_t bps = static_cast<int16_t>(bitsPerSample);
    file.write(reinterpret_cast<const char*>(&bps), 2);
    
    // data subchunk
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);
    file.write(reinterpret_cast<const char*>(samples.data()), dataSize);
}

} // anonymous namespace

namespace Aetherion::Assets {

// =============================================================================
// StubAssetGenerator
// =============================================================================

bool StubAssetGenerator::SupportsType(const std::string& assetType) const {
    const auto types = GetSupportedTypes();
    return std::find(types.begin(), types.end(), assetType) != types.end();
}

GenerationResult StubAssetGenerator::Generate(const GenerationRequest& request) {
    if (request.assetType == "texture") {
        return GenerateStubTexture(request);
    } else if (request.assetType == "mesh") {
        return GenerateStubMesh(request);
    } else if (request.assetType == "audio") {
        return GenerateStubAudio(request);
    } else if (request.assetType == "script") {
        return GenerateStubScript(request);
    }
    
    GenerationResult result;
    result.success = false;
    result.message = "Unsupported asset type: " + request.assetType;
    return result;
}

GenerationResult StubAssetGenerator::GenerateStubTexture(
    const GenerationRequest& request) {
    GenerationResult result;
    result.assetId = request.targetId.empty() ?
        ("stub_texture_" + request.requestId) : request.targetId;
    
    const int width = request.width > 0 ? request.width : 256;
    const int height = request.height > 0 ? request.height : 256;
    
    // Create a simple magenta/black checkerboard as a placeholder
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
            bool checker = ((x / 32) + (y / 32)) % 2 == 0;
            pixels[idx + 0] = checker ? 255 : 0;    // R
            pixels[idx + 1] = 0;                     // G
            pixels[idx + 2] = checker ? 255 : 0;    // B
            pixels[idx + 3] = 255;                  // A
        }
    }
    
    std::filesystem::path outPath = request.outputDir / (result.assetId + ".bmp");
    WriteBMP(outPath, pixels, width, height);
    
    result.success = true;
    result.outputPath = outPath;
    result.message = "Generated stub texture placeholder";
    result.diagnostics = "Prompt was: " + request.prompt;
    
    return result;
}

GenerationResult StubAssetGenerator::GenerateStubMesh(
    const GenerationRequest& request) {
    GenerationResult result;
    result.assetId = request.targetId.empty() ?
        ("stub_mesh_" + request.requestId) : request.targetId;
    
    // Create a simple cube
    std::vector<std::array<float, 3>> vertices = {
        {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
        {0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f}, {0.5f, -0.5f,  0.5f},
        {0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}
    };
    
    std::vector<std::array<int, 3>> faces = {
        {0, 1, 2}, {0, 2, 3},  // Front
        {4, 6, 5}, {4, 7, 6},  // Back
        {0, 4, 5}, {0, 5, 1},  // Bottom
        {2, 6, 7}, {2, 7, 3},  // Top
        {0, 3, 7}, {0, 7, 4},  // Left
        {1, 5, 6}, {1, 6, 2}   // Right
    };
    
    std::filesystem::path outPath = request.outputDir / (result.assetId + ".obj");
    WriteOBJ(outPath, vertices, faces);
    
    result.success = true;
    result.outputPath = outPath;
    result.message = "Generated stub mesh placeholder (cube)";
    result.diagnostics = "Prompt was: " + request.prompt;
    
    return result;
}

GenerationResult StubAssetGenerator::GenerateStubAudio(
    const GenerationRequest& request) {
    GenerationResult result;
    result.assetId = request.targetId.empty() ?
        ("stub_audio_" + request.requestId) : request.targetId;
    
    // Generate a simple sine wave
    const int sampleRate = 44100;
    const float duration = 0.5f;  // 0.5 seconds
    const float frequency = 440.0f;  // A4
    const int numSamples = static_cast<int>(sampleRate * duration);
    
    std::vector<int16_t> samples(static_cast<size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float value = std::sin(2.0f * 3.14159265f * frequency * t);
        // Apply envelope
        float envelope = 1.0f;
        if (t < 0.05f) envelope = t / 0.05f;
        else if (t > duration - 0.1f) envelope = (duration - t) / 0.1f;
        samples[static_cast<size_t>(i)] = static_cast<int16_t>(value * envelope * 32000);
    }
    
    std::filesystem::path outPath = request.outputDir / (result.assetId + ".wav");
    WriteWAV(outPath, samples, sampleRate);
    
    result.success = true;
    result.outputPath = outPath;
    result.message = "Generated stub audio placeholder (440Hz tone)";
    result.diagnostics = "Prompt was: " + request.prompt;
    
    return result;
}

GenerationResult StubAssetGenerator::GenerateStubScript(
    const GenerationRequest& request) {
    GenerationResult result;
    result.assetId = request.targetId.empty() ?
        ("stub_script_" + request.requestId) : request.targetId;
    
    std::ostringstream code;
    code << "# Auto-generated stub script\n";
    code << "# Prompt: " << request.prompt << "\n\n";
    code << "def update(entity, context):\n";
    code << "    \"\"\"Stub behavior generated from prompt.\"\"\"\n";
    code << "    return {\"state\": \"Idle\", \"actions\": []}\n";
    
    std::filesystem::path outPath = request.outputDir / (result.assetId + ".py");
    std::ofstream file(outPath);
    if (file.is_open()) {
        file << code.str();
    }
    
    result.success = true;
    result.outputPath = outPath;
    result.message = "Generated stub script placeholder";
    result.diagnostics = "Prompt was: " + request.prompt;
    
    return result;
}

// =============================================================================
// ProceduralTextureGenerator
// =============================================================================

GenerationResult ProceduralTextureGenerator::Generate(
    const GenerationRequest& request) {
    GenerationResult result;
    result.assetId = request.targetId.empty() ?
        ("proc_texture_" + request.requestId) : request.targetId;
    
    const int width = request.width > 0 ? request.width : 256;
    const int height = request.height > 0 ? request.height : 256;
    
    PatternType pattern = ParsePatternFromPrompt(request.prompt);
    auto color1 = ParseColorFromPrompt(request.prompt);
    std::array<uint8_t, 4> color2 = {0, 0, 0, 255};  // Default secondary color
    
    // Adjust secondary color based on pattern
    if (pattern == PatternType::Checker || pattern == PatternType::Grid) {
        color2 = {255, 255, 255, 255};
    } else if (pattern == PatternType::Brick) {
        color2 = {80, 80, 80, 255};  // Mortar color
    }
    
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    GeneratePattern(pixels, width, height, pattern, color1, color2);
    
    std::filesystem::path outPath = request.outputDir / (result.assetId + ".bmp");
    WriteBMP(outPath, pixels, width, height);
    
    result.success = true;
    result.outputPath = outPath;
    result.message = "Generated procedural texture";
    result.diagnostics = "Pattern: " + std::to_string(static_cast<int>(pattern));
    
    return result;
}

ProceduralTextureGenerator::PatternType
ProceduralTextureGenerator::ParsePatternFromPrompt(const std::string& prompt) const {
    std::string lower = ToLower(prompt);
    
    if (ContainsWord(lower, "checker") || ContainsWord(lower, "checkered")) {
        return PatternType::Checker;
    } else if (ContainsWord(lower, "gradient")) {
        return PatternType::Gradient;
    } else if (ContainsWord(lower, "noise") || ContainsWord(lower, "perlin")) {
        return PatternType::Noise;
    } else if (ContainsWord(lower, "brick")) {
        return PatternType::Brick;
    } else if (ContainsWord(lower, "grid")) {
        return PatternType::Grid;
    } else if (ContainsWord(lower, "stripe")) {
        return PatternType::Stripes;
    } else if (ContainsWord(lower, "dot") || ContainsWord(lower, "polka")) {
        return PatternType::Dots;
    }
    
    return PatternType::Noise;  // Default to noise
}

std::array<uint8_t, 4>
ProceduralTextureGenerator::ParseColorFromPrompt(const std::string& prompt) const {
    std::string lower = ToLower(prompt);
    
    if (ContainsWord(lower, "red")) return {200, 50, 50, 255};
    if (ContainsWord(lower, "green")) return {50, 200, 50, 255};
    if (ContainsWord(lower, "blue")) return {50, 50, 200, 255};
    if (ContainsWord(lower, "yellow")) return {200, 200, 50, 255};
    if (ContainsWord(lower, "orange")) return {230, 130, 30, 255};
    if (ContainsWord(lower, "purple")) return {150, 50, 200, 255};
    if (ContainsWord(lower, "pink")) return {255, 150, 200, 255};
    if (ContainsWord(lower, "brown") || ContainsWord(lower, "brick")) 
        return {160, 80, 50, 255};
    if (ContainsWord(lower, "gray") || ContainsWord(lower, "grey")) 
        return {128, 128, 128, 255};
    if (ContainsWord(lower, "white")) return {240, 240, 240, 255};
    if (ContainsWord(lower, "black")) return {20, 20, 20, 255};
    
    return {100, 150, 200, 255};  // Default blue-gray
}

void ProceduralTextureGenerator::GeneratePattern(
    std::vector<uint8_t>& pixels, int width, int height,
    PatternType pattern, const std::array<uint8_t, 4>& color1,
    const std::array<uint8_t, 4>& color2) const {
    
    auto setPixel = [&](int x, int y, const std::array<uint8_t, 4>& color) {
        size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
        pixels[idx + 0] = color[0];
        pixels[idx + 1] = color[1];
        pixels[idx + 2] = color[2];
        pixels[idx + 3] = color[3];
    };
    
    auto lerp = [](uint8_t a, uint8_t b, float t) -> uint8_t {
        return static_cast<uint8_t>(a + (b - a) * t);
    };
    
    auto blendColors = [&](const std::array<uint8_t, 4>& c1,
                          const std::array<uint8_t, 4>& c2, float t) {
        return std::array<uint8_t, 4>{
            lerp(c1[0], c2[0], t),
            lerp(c1[1], c2[1], t),
            lerp(c1[2], c2[2], t),
            255
        };
    };
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            std::array<uint8_t, 4> color = color1;
            
            switch (pattern) {
                case PatternType::Solid:
                    color = color1;
                    break;
                    
                case PatternType::Checker: {
                    int size = std::max(width, height) / 8;
                    bool check = ((x / size) + (y / size)) % 2 == 0;
                    color = check ? color1 : color2;
                    break;
                }
                
                case PatternType::Gradient: {
                    float t = static_cast<float>(y) / height;
                    color = blendColors(color1, color2, t);
                    break;
                }
                
                case PatternType::Noise: {
                    float scale = 0.02f;
                    float n = FractalNoise(x * scale, y * scale);
                    n = (n + 1.0f) * 0.5f;  // Normalize to 0-1
                    color = blendColors(color1, color2, n);
                    break;
                }
                
                case PatternType::Brick: {
                    int brickW = width / 8;
                    int brickH = height / 16;
                    int mortarSize = 2;
                    
                    int row = y / brickH;
                    int offset = (row % 2) * (brickW / 2);
                    int bx = (x + offset) % brickW;
                    int by = y % brickH;
                    
                    bool isMortar = (bx < mortarSize) || (by < mortarSize);
                    color = isMortar ? color2 : color1;
                    break;
                }
                
                case PatternType::Grid: {
                    int gridSize = std::max(width, height) / 16;
                    bool isLine = (x % gridSize < 2) || (y % gridSize < 2);
                    color = isLine ? color2 : color1;
                    break;
                }
                
                case PatternType::Stripes: {
                    int stripeW = width / 8;
                    bool stripe = (x / stripeW) % 2 == 0;
                    color = stripe ? color1 : color2;
                    break;
                }
                
                case PatternType::Dots: {
                    int spacing = std::max(width, height) / 8;
                    int radius = spacing / 4;
                    int cx = ((x + spacing / 2) / spacing) * spacing;
                    int cy = ((y + spacing / 2) / spacing) * spacing;
                    int dx = x - cx;
                    int dy = y - cy;
                    bool inDot = (dx * dx + dy * dy) < (radius * radius);
                    color = inDot ? color1 : color2;
                    break;
                }
            }
            
            setPixel(x, y, color);
        }
    }
}

// =============================================================================
// GenerationQueue
// =============================================================================

GenerationQueue::GenerationQueue() {
    // Register default generators
    RegisterGenerator(std::make_shared<StubAssetGenerator>());
    RegisterGenerator(std::make_shared<ProceduralTextureGenerator>());
}

GenerationQueue::~GenerationQueue() = default;

void GenerationQueue::RegisterGenerator(std::shared_ptr<IAssetGenerator> generator) {
    if (!generator) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if already registered
    for (const auto& gen : m_generators) {
        if (gen->GetName() == generator->GetName()) {
            return;
        }
    }
    
    generator->OnRegister();
    m_generators.push_back(std::move(generator));
}

void GenerationQueue::UnregisterGenerator(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::find_if(m_generators.begin(), m_generators.end(),
        [&name](const auto& gen) { return gen->GetName() == name; });
    
    if (it != m_generators.end()) {
        (*it)->OnUnregister();
        m_generators.erase(it);
    }
}

std::shared_ptr<IAssetGenerator> GenerationQueue::GetGenerator(
    const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& gen : m_generators) {
        if (gen->GetName() == name) {
            return gen;
        }
    }
    
    return nullptr;
}

std::shared_ptr<IAssetGenerator> GenerationQueue::GetGeneratorForType(
    const std::string& assetType) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Prefer procedural generators over stub
    for (const auto& gen : m_generators) {
        if (gen->SupportsType(assetType) && 
            gen->GetName() != "StubGenerator" &&
            gen->IsAvailable()) {
            return gen;
        }
    }
    
    // Fall back to stub
    for (const auto& gen : m_generators) {
        if (gen->SupportsType(assetType) && gen->IsAvailable()) {
            return gen;
        }
    }
    
    return nullptr;
}

std::string GenerationQueue::GenerateRequestId() {
    std::ostringstream ss;
    ss << "req_" << std::hex << std::setw(8) << std::setfill('0')
       << m_nextRequestId++;
    return ss.str();
}

std::string GenerationQueue::QueueRequest(GenerationRequest request,
                                          CompletionCallback onComplete) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (request.requestId.empty()) {
        request.requestId = GenerateRequestId();
    }
    
    if (request.outputDir.empty()) {
        request.outputDir = m_outputDirectory;
    }
    
    // Ensure output directory exists
    std::error_code ec;
    std::filesystem::create_directories(request.outputDir, ec);
    
    const std::string id = request.requestId;
    
    GenerationState state;
    state.requestId = id;
    state.status = GenerationStatus::Pending;
    state.statusMessage = "Queued for generation";
    state.queuedTime = GetCurrentTimeMs();
    
    m_requests[id] = std::move(request);
    m_states[id] = std::move(state);
    
    if (onComplete) {
        m_callbacks[id] = std::move(onComplete);
    }
    
    m_pendingQueue.push(id);
    
    return id;
}

bool GenerationQueue::CancelRequest(const std::string& requestId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_states.find(requestId);
    if (it == m_states.end()) {
        return false;
    }
    
    if (it->second.status == GenerationStatus::InProgress) {
        return false;  // Can't cancel in-progress
    }
    
    it->second.status = GenerationStatus::Cancelled;
    it->second.statusMessage = "Cancelled by user";
    
    return true;
}

bool GenerationQueue::RetryRequest(const std::string& requestId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto stateIt = m_states.find(requestId);
    if (stateIt == m_states.end()) {
        return false;
    }
    
    if (stateIt->second.status != GenerationStatus::Failed) {
        return false;
    }
    
    if (stateIt->second.retryCount >= stateIt->second.maxRetries) {
        return false;
    }
    
    stateIt->second.status = GenerationStatus::Pending;
    stateIt->second.statusMessage = "Retrying...";
    stateIt->second.retryCount++;
    stateIt->second.progress = 0.0f;
    
    m_pendingQueue.push(requestId);
    
    return true;
}

std::optional<GenerationState> GenerationQueue::GetRequestState(
    const std::string& requestId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_states.find(requestId);
    if (it == m_states.end()) {
        return std::nullopt;
    }
    
    return it->second;
}

std::vector<std::string> GenerationQueue::GetPendingRequests() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> result;
    for (const auto& [id, state] : m_states) {
        if (state.status == GenerationStatus::Pending ||
            state.status == GenerationStatus::InProgress) {
            result.push_back(id);
        }
    }
    
    return result;
}

std::vector<std::string> GenerationQueue::GetCompletedRequests() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> result;
    for (const auto& [id, state] : m_states) {
        if (state.status == GenerationStatus::Completed) {
            result.push_back(id);
        }
    }
    
    return result;
}

std::vector<std::string> GenerationQueue::GetFailedRequests() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> result;
    for (const auto& [id, state] : m_states) {
        if (state.status == GenerationStatus::Failed) {
            result.push_back(id);
        }
    }
    
    return result;
}

bool GenerationQueue::ProcessNext() {
    std::string requestId;
    GenerationRequest request;
    std::shared_ptr<IAssetGenerator> generator;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Find next non-cancelled request
        while (!m_pendingQueue.empty()) {
            requestId = m_pendingQueue.front();
            m_pendingQueue.pop();
            
            auto stateIt = m_states.find(requestId);
            if (stateIt != m_states.end() &&
                stateIt->second.status == GenerationStatus::Pending) {
                break;
            }
            requestId.clear();
        }
        
        if (requestId.empty()) {
            return false;
        }
        
        auto reqIt = m_requests.find(requestId);
        if (reqIt == m_requests.end()) {
            return false;
        }
        
        request = reqIt->second;
        generator = GetGeneratorForType(request.assetType);
        
        if (!generator) {
            m_states[requestId].status = GenerationStatus::Failed;
            m_states[requestId].statusMessage = 
                "No generator available for type: " + request.assetType;
            return false;
        }
        
        m_states[requestId].status = GenerationStatus::InProgress;
        m_states[requestId].startTime = GetCurrentTimeMs();
        m_states[requestId].statusMessage = "Generating with " + generator->GetName();
    }
    
    NotifyProgress(requestId, 0.1f, "Starting generation...");
    
    // Generate (outside lock)
    GenerationResult result = generator->Generate(request);
    result.generationTimeMs = GetCurrentTimeMs() - m_states[requestId].startTime;
    
    CompletionCallback callback;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto& state = m_states[requestId];
        state.endTime = GetCurrentTimeMs();
        state.result = result;
        
        if (result.success) {
            state.status = GenerationStatus::Completed;
            state.progress = 1.0f;
            state.statusMessage = result.message;
        } else {
            state.status = GenerationStatus::Failed;
            state.statusMessage = result.message;
        }
        
        auto cbIt = m_callbacks.find(requestId);
        if (cbIt != m_callbacks.end()) {
            callback = cbIt->second;
        }
    }
    
    NotifyProgress(requestId, 1.0f, result.message);
    
    if (callback) {
        callback(result);
    }
    
    return true;
}

void GenerationQueue::ProcessAll() {
    while (ProcessNext()) {
        // Continue processing
    }
}

void GenerationQueue::ClearHistory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> toRemove;
    
    for (const auto& [id, state] : m_states) {
        if (state.status == GenerationStatus::Completed ||
            state.status == GenerationStatus::Failed ||
            state.status == GenerationStatus::Cancelled) {
            toRemove.push_back(id);
        }
    }
    
    for (const auto& id : toRemove) {
        m_states.erase(id);
        m_requests.erase(id);
        m_callbacks.erase(id);
    }
}

size_t GenerationQueue::GetPendingCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& [id, state] : m_states) {
        if (state.status == GenerationStatus::Pending ||
            state.status == GenerationStatus::InProgress) {
            ++count;
        }
    }
    return count;
}

size_t GenerationQueue::GetCompletedCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& [id, state] : m_states) {
        if (state.status == GenerationStatus::Completed) {
            ++count;
        }
    }
    return count;
}

size_t GenerationQueue::GetFailedCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& [id, state] : m_states) {
        if (state.status == GenerationStatus::Failed) {
            ++count;
        }
    }
    return count;
}

void GenerationQueue::SetOutputDirectory(std::filesystem::path dir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_outputDirectory = std::move(dir);
    
    std::error_code ec;
    std::filesystem::create_directories(m_outputDirectory, ec);
}

void GenerationQueue::UpdateState(const std::string& requestId,
                                  GenerationStatus status,
                                  float progress,
                                  const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_states.find(requestId);
    if (it != m_states.end()) {
        it->second.status = status;
        it->second.progress = progress;
        if (!message.empty()) {
            it->second.statusMessage = message;
        }
    }
}

void GenerationQueue::NotifyProgress(const std::string& requestId,
                                    float progress,
                                    const std::string& message) {
    if (m_progressCallback) {
        m_progressCallback(requestId, progress, message);
    }
}

// =============================================================================
// AssetGeneratorFactory
// =============================================================================

AssetGeneratorFactory& AssetGeneratorFactory::Instance() {
    static AssetGeneratorFactory instance;
    return instance;
}

void AssetGeneratorFactory::RegisterCreator(const std::string& name,
                                           GeneratorCreator creator) {
    m_creators[name] = std::move(creator);
}

std::shared_ptr<IAssetGenerator> AssetGeneratorFactory::Create(
    const std::string& name) const {
    auto it = m_creators.find(name);
    if (it != m_creators.end()) {
        return it->second();
    }
    return nullptr;
}

std::vector<std::string> AssetGeneratorFactory::GetAvailableGenerators() const {
    std::vector<std::string> result;
    result.reserve(m_creators.size());
    for (const auto& [name, _] : m_creators) {
        result.push_back(name);
    }
    return result;
}

} // namespace Aetherion::Assets
