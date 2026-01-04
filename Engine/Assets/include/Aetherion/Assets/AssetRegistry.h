#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Aetherion/Assets/Material.h"

namespace Aetherion::Assets {
class AssetRegistry {
public:
  AssetRegistry() = default;
  ~AssetRegistry() = default;

  void Scan(const std::string &rootPath);
  void Rescan();
  [[nodiscard]] bool HasAsset(const std::string &assetId) const;
  enum class AssetType { Texture, Mesh, Audio, Script, Scene, Shader, Other };

  static const char *AssetTypeToString(AssetType type);

  struct AssetEntry {
    std::string id;
    std::filesystem::path path;
    AssetType type{AssetType::Other};
  };

  struct VirtualAsset {
    AssetEntry entry;
    std::string uri;
    bool ready{false};
    std::string status{"Pending generation"};
  };

  [[nodiscard]] const std::vector<AssetEntry> &GetEntries() const noexcept;
  [[nodiscard]] const std::filesystem::path &GetRootPath() const noexcept;
  [[nodiscard]] const AssetEntry *
  FindEntry(const std::string &assetId) const noexcept;

  struct CachedTexture {
    std::string id;
    std::filesystem::path path;
  };

  struct MeshData {
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

  struct CachedMesh {
    std::string id;
    std::filesystem::path source;
    std::vector<std::string> textureIds;
    std::vector<std::string> materialIds;
  };

  struct MeshImportSettings {
    float scale{1.0f};
    bool centerMesh{false};
    bool generateNormals{false};
    bool generateTangents{false};
    bool flipUVs{false};
    bool flipWinding{false};
    bool optimize{false};
  };

  struct TextureImportSettings {
    bool srgb{true};
    bool generateMipmaps{true};
    bool flipVertical{false};
    bool isNormalMap{false};
  };

  struct GltfImportResult {
    bool success{false};
    std::string id;
    std::vector<std::string> textures;
    std::vector<std::string> materials;
    std::string message;
  };

  [[nodiscard]] GltfImportResult ImportGltf(const std::string &gltfPath,
                                            bool forceReimport = false);
  [[nodiscard]] const CachedMesh *GetMesh(const std::string &id) const;
  [[nodiscard]] const CachedTexture *GetTexture(const std::string &id) const;
  [[nodiscard]] const Material *GetMaterial(const std::string &id) const;
  [[nodiscard]] Material *GetMaterialMutable(const std::string &id);
  Material *CreateMaterial(const std::string &name);
  bool SaveMaterial(const std::string &assetId);
  [[nodiscard]] MeshImportSettings
  GetMeshImportSettings(const std::string &assetId) const;
  bool SetMeshImportSettings(const std::string &assetId,
                             const MeshImportSettings &settings);
  bool ReimportMeshAsset(const std::string &assetId,
                         std::string *outMessage = nullptr);
  [[nodiscard]] TextureImportSettings
  GetTextureImportSettings(const std::string &assetId) const;
  bool SetTextureImportSettings(const std::string &assetId,
                                const TextureImportSettings &settings);
  bool ReimportTextureAsset(const std::string &assetId,
                            std::string *outMessage = nullptr);
  [[nodiscard]] const MeshData *
  GetMeshData(const std::string &assetId) const noexcept;
  [[nodiscard]] const MeshData *LoadMeshData(const std::string &assetId);
  [[nodiscard]] const std::vector<std::string> *
  GetAssetDependencies(const std::string &assetId) const noexcept;

  struct AssetChange {
    enum class Kind { Added, Modified, Removed, Moved, Metadata };

    std::string id;
    AssetType type{AssetType::Other};
    Kind kind{Kind::Modified};
    std::uint64_t serial{0};
  };

  [[nodiscard]] std::uint64_t GetChangeSerial() const noexcept;
  void GetChangesSince(std::uint64_t serial,
                       std::vector<AssetChange> &out) const;

  static std::filesystem::path
  GetMetadataPathForAsset(const std::filesystem::path &assetPath);

  void RegisterVirtualAsset(const std::string &uri, AssetType type,
                            const std::filesystem::path &cachePath = {});
  [[nodiscard]] const std::unordered_map<std::string, VirtualAsset> &
  GetVirtualAssets() const noexcept;
  [[nodiscard]] bool IsVirtualAsset(const std::string &assetId) const noexcept;

  // Generative asset support
  enum class GenerativeAssetStatus {
    Pending,
    Generating,
    Ready,
    Failed
  };

  struct GenerativeAssetInfo {
    std::string assetId;
    std::string prompt;
    AssetType type{AssetType::Other};
    GenerativeAssetStatus status{GenerativeAssetStatus::Pending};
    std::string statusMessage;
    std::filesystem::path outputPath;
    float progress{0.0f};
    std::uint64_t requestedTime{0};
    std::uint64_t completedTime{0};
  };

  /// @brief Request generation of a new asset from a prompt
  /// @return Asset ID that can be used for tracking
  std::string RequestGenerativeAsset(const std::string &prompt, AssetType type,
                                     const std::string &suggestedName = {});

  /// @brief Update the status of a generative asset
  void UpdateGenerativeAssetStatus(const std::string &assetId,
                                   GenerativeAssetStatus status,
                                   const std::string &message = {},
                                   const std::filesystem::path &outputPath = {});

  /// @brief Mark a generative asset as ready with its generated file
  void FinalizeGenerativeAsset(const std::string &assetId,
                               const std::filesystem::path &generatedPath);

  /// @brief Get info about a generative asset
  [[nodiscard]] const GenerativeAssetInfo *
  GetGenerativeAssetInfo(const std::string &assetId) const noexcept;

  /// @brief Get all generative assets
  [[nodiscard]] const std::unordered_map<std::string, GenerativeAssetInfo> &
  GetGenerativeAssets() const noexcept;

  /// @brief Get generative assets filtered by status
  [[nodiscard]] std::vector<std::string>
  GetGenerativeAssetsByStatus(GenerativeAssetStatus status) const;

  // TODO: Replace string identifiers with strong asset handles/UUIDs.
  // TODO: Add import pipeline hooks and metadata caching.
private:
  std::unordered_map<std::string, std::string> m_placeholderAssets;
  std::unordered_map<std::string, CachedMesh> m_meshes;
  std::unordered_map<std::string, CachedTexture> m_textures;
  std::unordered_map<std::string, Material> m_materials;
  std::unordered_map<std::string, MeshData> m_meshData;
  std::unordered_map<std::string, VirtualAsset> m_virtualAssets;
  std::unordered_map<std::string, GenerativeAssetInfo> m_generativeAssets;
  std::filesystem::path m_rootPath;
  std::vector<AssetEntry> m_entries;
  std::unordered_map<std::string, size_t> m_entryLookup;
  std::unordered_map<std::string, std::string> m_pathToId;
  std::unordered_map<std::string, std::vector<std::string>> m_assetDependencies;

  struct FileState {
    std::filesystem::path path;
    std::filesystem::file_time_type assetTime{};
    std::filesystem::file_time_type metaTime{};
  };

  std::unordered_map<std::string, FileState> m_fileStates;
  std::vector<AssetChange> m_changeLog;
  std::uint64_t m_changeSerial{0};
};
} // namespace Aetherion::Assets
