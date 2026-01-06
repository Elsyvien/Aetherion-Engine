#pragma once

#include <vector>
#include <string>
#include <memory>

namespace Aetherion::Assets
{
    // Represents a compact neural representation of an asset
    struct LatentAsset
    {
        std::vector<float> data; // The latent vector
        std::string modelId;     // The generative model ID (e.g. "stable-diffusion-v4-latent")
        std::vector<int64_t> dimensions; // Dimensions of the latent space
        
        // Metadata for reconstruction
        std::string originalPrompt;
        float guidanceScale{7.5f};
    };

    // Interface for decoding latent assets into usable resources
    class LatentDecoder
    {
    public:
        // Decodes a latent vector into raw image data (RGBA)
        // In a real implementation, this would run a VAE decoder (possibly on GPU)
        static std::vector<uint8_t> DecodeToImage(const LatentAsset& asset, int& outWidth, int& outHeight);
        
        // Decodes a latent vector into mesh data
        static void DecodeToMesh(const LatentAsset& asset, std::vector<float>& outVertices, std::vector<uint32_t>& outIndices);
    };
}
