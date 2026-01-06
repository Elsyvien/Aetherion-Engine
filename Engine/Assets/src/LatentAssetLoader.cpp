#include "Aetherion/Assets/LatentAssetLoader.h"
#include <fstream>
#include <sstream>
#include <random>

namespace Aetherion::Assets
{
    std::shared_ptr<LatentAsset> LatentAssetLoader::LoadTyped(const std::filesystem::path& path, std::string& outError)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            outError = "Failed to open file";
            return nullptr;
        }

        auto asset = std::make_shared<LatentAsset>();
        
        // Simple text format for prototype:
        // ModelID
        // Prompt
        // v1 v2 v3 ...
        
        std::getline(file, asset->modelId);
        std::getline(file, asset->originalPrompt);

        float val;
        while (file >> val)
        {
            asset->data.push_back(val);
        }

        if (asset->data.empty())
        {
            // Fallback for empty/missing files during dev
            asset->data.resize(64 * 64 * 4, 0.5f); 
            asset->modelId = "debug-model";
        }

        return asset;
    }

    std::vector<uint8_t> LatentDecoder::DecodeToImage(const LatentAsset& asset, int& outWidth, int& outHeight)
    {
        // Mock Decoding: Generate a noise texture based on the latent vector
        outWidth = 256;
        outHeight = 256;
        std::vector<uint8_t> pixels(outWidth * outHeight * 4);

        std::mt19937 rng(12345); // Deterministic based on nothing for now, ideally hash of asset.data
        if (!asset.data.empty()) {
             rng.seed(static_cast<unsigned int>(asset.data[0] * 10000));
        }

        std::uniform_int_distribution<int> dist(0, 255);

        for (size_t i = 0; i < pixels.size(); ++i)
        {
            pixels[i] = static_cast<uint8_t>(dist(rng));
        }
        
        // "Optimize on-the-fly decoding" -> This is where the neural net forward pass would happen.
        return pixels;
    }

    void LatentDecoder::DecodeToMesh(const LatentAsset& asset, std::vector<float>& outVertices, std::vector<uint32_t>& outIndices)
    {
        // Mock Decoding: Create a simple sphere or cube
        // ...
    }
}
