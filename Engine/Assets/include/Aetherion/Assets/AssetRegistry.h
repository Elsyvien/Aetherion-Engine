#pragma once

#include <array>
#include <cstdint>
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
    void Rescan();
    [[nodiscard]] bool HasAsset(const std::string& assetId) const;
    enum class AssetType
    {
        Texture,
        Mesh,
        Audio,
        Script,
        Scene,
        Shader,
        Other
    };

    struct AssetEntry
    {
        std::string id;
        std::filesystem::path path;
        AssetType type{AssetType::Other};
    };

    [[nodiscard]] const std::vector<AssetEntry>& GetEntries() const noexcept;
    [[nodiscard]] const std::filesystem::path& GetRootPath() const noexcept;
    [[nodiscard]] const AssetEntry* FindEntry(const std::string& assetId) const noexcept;

    struct CachedTexture
    {
        std::string id;
        std::filesystem::path path;
    };

    struct MeshData
    {
        std::vector<std::array<float, 3>> positions;
        std::vector<std::array<float, 4>> colors; // RGBA vertex colors
        std::vector<std::uint32_t> indices;
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
    [[nodiscard]] const MeshData* GetMeshData(const std::string& assetId) const noexcept;
    [[nodiscard]] const MeshData* LoadMeshData(const std::string& assetId);

    // TODO: Replace string identifiers with strong asset handles/UUIDs.
    // TODO: Add import pipeline hooks and metadata caching.
private:
    std::unordered_map<std::string, std::string> m_placeholderAssets;
    std::unordered_map<std::string, CachedMesh> m_meshes;
    std::unordered_map<std::string, CachedTexture> m_textures;
    std::unordered_map<std::string, MeshData> m_meshData;
    std::filesystem::path m_rootPath;
    std::vector<AssetEntry> m_entries;
    std::unordered_map<std::string, size_t> m_entryLookup;
};
} // namespace Aetherion::Assets
