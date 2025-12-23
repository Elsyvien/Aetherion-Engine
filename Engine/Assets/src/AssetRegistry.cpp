#include "Aetherion/Assets/AssetRegistry.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>

namespace
{
using namespace Aetherion::Assets;

std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool PathHasSegment(const std::filesystem::path& path, const std::string& segment)
{
    for (const auto& part : path)
    {
        if (part == segment)
        {
            return true;
        }
    }
    return false;
}

AssetRegistry::AssetType ClassifyAssetType(const std::filesystem::path& path)
{
    const std::string ext = ToLower(path.extension().string());
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" ||
        ext == ".gif" || ext == ".dds" || ext == ".ktx" || ext == ".ktx2")
    {
        return AssetRegistry::AssetType::Texture;
    }
    if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx" || ext == ".dae")
    {
        return AssetRegistry::AssetType::Mesh;
    }
    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac" || ext == ".aiff")
    {
        return AssetRegistry::AssetType::Audio;
    }
    if (ext == ".lua" || ext == ".py" || ext == ".js" || ext == ".cs")
    {
        return AssetRegistry::AssetType::Script;
    }
    if (ext == ".vert" || ext == ".frag" || ext == ".glsl" || ext == ".spv")
    {
        return AssetRegistry::AssetType::Shader;
    }
    if (ext == ".json")
    {
        return PathHasSegment(path, "scenes") ? AssetRegistry::AssetType::Scene
                                              : AssetRegistry::AssetType::Other;
    }
    return AssetRegistry::AssetType::Other;
}

int AssetTypeOrder(AssetRegistry::AssetType type)
{
    switch (type)
    {
    case AssetRegistry::AssetType::Texture:
        return 0;
    case AssetRegistry::AssetType::Mesh:
        return 1;
    case AssetRegistry::AssetType::Audio:
        return 2;
    case AssetRegistry::AssetType::Script:
        return 3;
    case AssetRegistry::AssetType::Scene:
        return 4;
    case AssetRegistry::AssetType::Shader:
        return 5;
    case AssetRegistry::AssetType::Other:
    default:
        return 6;
    }
}
} // namespace

namespace Aetherion::Assets
{
void AssetRegistry::Scan(const std::string& rootPath)
{
    m_placeholderAssets.clear();
    m_placeholderAssets.emplace("root", rootPath);
    m_entries.clear();
    m_entryLookup.clear();
    m_rootPath = std::filesystem::path(rootPath);

    std::error_code ec;
    if (!std::filesystem::exists(m_rootPath, ec))
    {
        return;
    }

    const auto options = std::filesystem::directory_options::skip_permission_denied;
    for (auto it = std::filesystem::recursive_directory_iterator(m_rootPath, options, ec);
         it != std::filesystem::recursive_directory_iterator();
         it.increment(ec))
    {
        if (ec)
        {
            ec.clear();
            continue;
        }

        const auto& entry = *it;
        if (!entry.is_regular_file(ec))
        {
            continue;
        }

        const auto filename = entry.path().filename().string();
        if (!filename.empty() && filename.front() == '.')
        {
            continue;
        }

        std::filesystem::path relative = std::filesystem::relative(entry.path(), m_rootPath, ec);
        std::string id = (!ec && !relative.empty())
                             ? relative.generic_string()
                             : entry.path().filename().generic_string();
        if (id.empty())
        {
            continue;
        }

        AssetEntry asset{};
        asset.id = std::move(id);
        asset.path = entry.path();
        asset.type = ClassifyAssetType(entry.path());

        m_entryLookup.emplace(asset.id, m_entries.size());
        m_entries.push_back(std::move(asset));
    }

    std::sort(m_entries.begin(), m_entries.end(), [](const AssetEntry& a, const AssetEntry& b) {
        const int orderA = AssetTypeOrder(a.type);
        const int orderB = AssetTypeOrder(b.type);
        if (orderA != orderB)
        {
            return orderA < orderB;
        }
        return a.id < b.id;
    });

    m_entryLookup.clear();
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        m_entryLookup.emplace(m_entries[i].id, i);
    }
}

bool AssetRegistry::HasAsset(const std::string& assetId) const
{
    return m_placeholderAssets.find(assetId) != m_placeholderAssets.end() ||
           m_entryLookup.find(assetId) != m_entryLookup.end() || m_meshes.find(assetId) != m_meshes.end() ||
           m_textures.find(assetId) != m_textures.end();
}

const std::vector<AssetRegistry::AssetEntry>& AssetRegistry::GetEntries() const noexcept
{
    return m_entries;
}

const std::filesystem::path& AssetRegistry::GetRootPath() const noexcept
{
    return m_rootPath;
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
