#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
        std::vector<std::array<float, 3>> normals;
        std::vector<std::array<float, 4>> colors; // RGBA vertex colors
        std::vector<std::array<float, 2>> uvs;
        std::vector<std::array<float, 4>> tangents; // XYZW tangent with handedness
        std::vector<std::uint32_t> indices;
        std::array<float, 3> boundsMin{0.0f, 0.0f, 0.0f};
        std::array<float, 3> boundsMax{0.0f, 0.0f, 0.0f};
        std::array<float, 3> boundsCenter{0.0f, 0.0f, 0.0f};
        float boundsRadius{0.0f};
    };

    struct CachedMesh
    {
        std::string id;
        std::filesystem::path source;
        std::vector<std::string> textureIds;
        std::vector<std::string> materialIds;
    };

    struct CachedMaterial
    {
        std::string id;
        std::string name;
        std::array<float, 4> baseColor{1.0f, 1.0f, 1.0f, 1.0f};
        float metallic{0.0f};
        float roughness{1.0f};
        std::string albedoTextureId;
    };

    struct GltfImportResult
    {
        bool success{false};
        std::string id;
        std::vector<std::string> textures;
        std::vector<std::string> materials;
        std::string message;
    };

    [[nodiscard]] GltfImportResult ImportGltf(const std::string& gltfPath, bool forceReimport = false);
    [[nodiscard]] const CachedMesh* GetMesh(const std::string& id) const;
    [[nodiscard]] const CachedTexture* GetTexture(const std::string& id) const;
    [[nodiscard]] const CachedMaterial* GetMaterial(const std::string& id) const;
    [[nodiscard]] const MeshData* GetMeshData(const std::string& assetId) const noexcept;
    [[nodiscard]] const MeshData* LoadMeshData(const std::string& assetId);

    struct AssetChange
    {
        enum class Kind
        {
            Added,
            Modified,
            Removed,
            Moved,
            Metadata
        };

        std::string id;
        AssetType type{AssetType::Other};
        Kind kind{Kind::Modified};
        std::uint64_t serial{0};
    };

    [[nodiscard]] std::uint64_t GetChangeSerial() const noexcept;
    void GetChangesSince(std::uint64_t serial, std::vector<AssetChange>& out) const;

    static std::filesystem::path GetMetadataPathForAsset(const std::filesystem::path& assetPath);

    // TODO: Replace string identifiers with strong asset handles/UUIDs.
    // TODO: Add import pipeline hooks and metadata caching.
private:
    std::unordered_map<std::string, std::string> m_placeholderAssets;
    std::unordered_map<std::string, CachedMesh> m_meshes;
    std::unordered_map<std::string, CachedTexture> m_textures;
    std::unordered_map<std::string, CachedMaterial> m_materials;
    std::unordered_map<std::string, MeshData> m_meshData;
    std::filesystem::path m_rootPath;
    std::vector<AssetEntry> m_entries;
    std::unordered_map<std::string, size_t> m_entryLookup;
    std::unordered_map<std::string, std::string> m_pathToId;

    struct FileState
    {
        std::filesystem::path path;
        std::filesystem::file_time_type assetTime{};
        std::filesystem::file_time_type metaTime{};
    };

    std::unordered_map<std::string, FileState> m_fileStates;
    std::vector<AssetChange> m_changeLog;
    std::uint64_t m_changeSerial{0};
};
} // namespace Aetherion::Assets
