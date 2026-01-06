#pragma once

#include "Aetherion/AI/VisionModel.h"

namespace Aetherion::AI
{
    class LocalVisionModel : public IVisionModel
    {
    public:
        LocalVisionModel();
        ~LocalVisionModel() override;

        std::future<VisionAnalysisResult> AnalyzeImageAsync(const std::vector<uint8_t>& imageData, int width, int height, int channels) override;
        std::future<VisionAnalysisResult> AnalyzeImageFileAsync(const std::string& filePath) override;

    private:
        // Internal state for the model (e.g., loaded weights, ONNX session, etc.)
        bool m_isLoaded{false};
    };
}
