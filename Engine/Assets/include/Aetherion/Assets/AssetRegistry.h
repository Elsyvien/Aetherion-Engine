#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Aetherion::Assets
{
class AssetRegistry
{
public:
    AssetRegistry() = default;
    ~AssetRegistry() = default;

    void Scan(const std::string& rootPath);
    [[nodiscard]] bool HasAsset(const std::string& assetId) const;

    struct CachedTexture
    {
        std::string id;
        std::filesystem::path path;
    };

    struct CachedMesh
    {
        std::string id;
        std::filesystem::path source;
        std::vector<std::string> textureIds;
    };

    struct GltfImportResult
    {
        bool success{false};
        std::string id;
        std::vector<std::string> textures;
        std::string message;
    };

    [[nodiscard]] GltfImportResult ImportGltf(const std::string& gltfPath);
    [[nodiscard]] const CachedMesh* GetMesh(const std::string& id) const;
    [[nodiscard]] const CachedTexture* GetTexture(const std::string& id) const;

    // TODO: Replace string identifiers with strong asset handles/UUIDs.
    // TODO: Add import pipeline hooks and metadata caching.
private:
    std::unordered_map<std::string, std::string> m_placeholderAssets;
    std::unordered_map<std::string, CachedMesh> m_meshes;
    std::unordered_map<std::string, CachedTexture> m_textures;
};
} // namespace Aetherion::Assets
