#pragma once

#include "Aetherion/Assets/ResourceManager.h"
#include "Aetherion/Assets/LatentAsset.h"

namespace Aetherion::Assets
{
    class LatentAssetLoader : public ResourceLoader<LatentAsset>
    {
    public:
        [[nodiscard]] std::vector<std::string> GetSupportedExtensions() const override
        {
            return { ".latent", ".emb" };
        }

    protected:
        [[nodiscard]] std::shared_ptr<LatentAsset> LoadTyped(
            const std::filesystem::path& path,
            std::string& outError) override;

        [[nodiscard]] size_t EstimateSizeTyped(const std::shared_ptr<LatentAsset>& resource) const override
        {
            if (!resource) return 0;
            return resource->data.size() * sizeof(float) + sizeof(LatentAsset);
        }
    };
}
