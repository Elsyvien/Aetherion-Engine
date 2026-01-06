#pragma once

#include <string>
#include <vector>
#include <future>
#include <cstdint>

namespace Aetherion::AI
{
    struct VisionAnalysisResult
    {
        std::string description;
        std::vector<std::string> detectedTags;
        float confidence;
    };

    class IVisionModel
    {
    public:
        virtual ~IVisionModel() = default;

        // Analyze an image buffer (e.g., PNG/JPG data or raw pixels)
        virtual std::future<VisionAnalysisResult> AnalyzeImageAsync(const std::vector<uint8_t>& imageData, int width, int height, int channels) = 0;
        
        // Analyze an image from a file path
        virtual std::future<VisionAnalysisResult> AnalyzeImageFileAsync(const std::string& filePath) = 0;
    };
}
