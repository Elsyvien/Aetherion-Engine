#include "Aetherion/Assets/AssetRegistry.h"

#include <fstream>
#include <regex>
#include <iterator>
#include <filesystem>

namespace Aetherion::Assets
{
void AssetRegistry::Scan(const std::string& rootPath)
{
    // TODO: Implement directory traversal and asset discovery.
    m_placeholderAssets.clear();
    m_placeholderAssets.emplace("root", rootPath);
}

bool AssetRegistry::HasAsset(const std::string& assetId) const
{
    return m_placeholderAssets.find(assetId) != m_placeholderAssets.end() || m_meshes.find(assetId) != m_meshes.end() ||
           m_textures.find(assetId) != m_textures.end();
}

AssetRegistry::GltfImportResult AssetRegistry::ImportGltf(const std::string& gltfPath)
{
    GltfImportResult result{};

    std::filesystem::path source(gltfPath);
    if (!std::filesystem::exists(source))
    {
        result.message = "GLTF file not found";
        return result;
    }

    source = std::filesystem::absolute(source);
    const std::string id = source.stem().string();

    // Return cached entry if available.
    if (auto cached = m_meshes.find(id); cached != m_meshes.end())
    {
        result.success = true;
        result.id = id;
        result.textures = cached->second.textureIds;
        result.message = "Cached GLTF";
        return result;
    }

    std::ifstream input(source, std::ios::binary);
    if (!input.is_open())
    {
        result.message = "Unable to open GLTF";
        return result;
    }

    std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();

    // Naive texture URI extraction; sufficient for editor preview/caching.
    std::regex uriRegex("\\\"uri\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");
    std::smatch match;
    std::string::const_iterator searchStart(content.cbegin());

    CachedMesh mesh{};
    mesh.id = id;
    mesh.source = source;

    while (std::regex_search(searchStart, content.cend(), match, uriRegex))
    {
        if (match.size() >= 2)
        {
            std::filesystem::path texPath = source.parent_path() / match[1].str();
            const std::string texId = texPath.filename().string();
            mesh.textureIds.push_back(texId);

            if (m_textures.find(texId) == m_textures.end())
            {
                CachedTexture tex{};
                tex.id = texId;
                tex.path = texPath;
                m_textures.emplace(texId, tex);
            }
        }

        searchStart = match.suffix().first;
    }

    m_meshes.emplace(id, mesh);

    result.success = true;
    result.id = id;
    result.textures = mesh.textureIds;
    result.message = "Imported GLTF";
    return result;
}

const AssetRegistry::CachedMesh* AssetRegistry::GetMesh(const std::string& id) const
{
    auto it = m_meshes.find(id);
    if (it == m_meshes.end())
    {
        return nullptr;
    }
    return &it->second;
}

const AssetRegistry::CachedTexture* AssetRegistry::GetTexture(const std::string& id) const
{
    auto it = m_textures.find(id);
    if (it == m_textures.end())
    {
        return nullptr;
    }
    return &it->second;
}
} // namespace Aetherion::Assets
