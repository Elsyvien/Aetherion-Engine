#include "Aetherion/Assets/AssetRegistry.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

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

void TransformPosition(const cgltf_float* matrix, const float in[3], float out[3])
{
    out[0] = static_cast<float>(matrix[0] * in[0] + matrix[4] * in[1] + matrix[8] * in[2] + matrix[12]);
    out[1] = static_cast<float>(matrix[1] * in[0] + matrix[5] * in[1] + matrix[9] * in[2] + matrix[13]);
    out[2] = static_cast<float>(matrix[2] * in[0] + matrix[6] * in[1] + matrix[10] * in[2] + matrix[14]);
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
    m_entries.clear();
    m_entryLookup.clear();
    m_meshData.clear();

    std::error_code ec;
    m_rootPath = std::filesystem::absolute(rootPath, ec);
    if (ec)
    {
        ec.clear();
        m_rootPath = std::filesystem::path(rootPath);
    }
    m_placeholderAssets.emplace("root", m_rootPath.string());

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

void AssetRegistry::Rescan()
{
    if (m_rootPath.empty())
    {
        Scan("assets");
        return;
    }

    Scan(m_rootPath.string());
}

bool AssetRegistry::HasAsset(const std::string& assetId) const
{
    return m_placeholderAssets.find(assetId) != m_placeholderAssets.end() ||
           m_entryLookup.find(assetId) != m_entryLookup.end() || m_meshes.find(assetId) != m_meshes.end() ||
           m_textures.find(assetId) != m_textures.end() ||
           m_meshData.find(assetId) != m_meshData.end();
}

const std::vector<AssetRegistry::AssetEntry>& AssetRegistry::GetEntries() const noexcept
{
    return m_entries;
}

const std::filesystem::path& AssetRegistry::GetRootPath() const noexcept
{
    return m_rootPath;
}

const AssetRegistry::AssetEntry* AssetRegistry::FindEntry(const std::string& assetId) const noexcept
{
    auto it = m_entryLookup.find(assetId);
    if (it == m_entryLookup.end())
    {
        return nullptr;
    }
    const size_t index = it->second;
    if (index >= m_entries.size())
    {
        return nullptr;
    }
    return &m_entries[index];
}

const AssetRegistry::MeshData* AssetRegistry::GetMeshData(const std::string& assetId) const noexcept
{
    auto it = m_meshData.find(assetId);
    if (it == m_meshData.end())
    {
        return nullptr;
    }
    return &it->second;
}

const AssetRegistry::MeshData* AssetRegistry::LoadMeshData(const std::string& assetId)
{
    if (assetId.empty())
    {
        return nullptr;
    }

    if (const auto* cached = GetMeshData(assetId))
    {
        return cached;
    }

    std::filesystem::path sourcePath;
    if (const auto* entry = FindEntry(assetId))
    {
        sourcePath = entry->path;
    }
    else
    {
        sourcePath = std::filesystem::path(assetId);
        if (!sourcePath.is_absolute() && !m_rootPath.empty())
        {
            sourcePath = m_rootPath / sourcePath;
        }
    }

    std::error_code ec;
    if (sourcePath.empty() || !std::filesystem::exists(sourcePath, ec))
    {
        return nullptr;
    }

    const std::string extension = ToLower(sourcePath.extension().string());
    if (extension != ".gltf" && extension != ".glb")
    {
        return nullptr;
    }

    cgltf_options options{};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, sourcePath.string().c_str(), &data);
    if (result != cgltf_result_success || !data)
    {
        if (data)
        {
            cgltf_free(data);
        }
        return nullptr;
    }

    result = cgltf_load_buffers(&options, data, sourcePath.string().c_str());
    if (result != cgltf_result_success)
    {
        cgltf_free(data);
        return nullptr;
    }

    MeshData mesh{};

    auto appendPrimitive = [&](const cgltf_primitive& primitive, const cgltf_float* matrix) {
        if (primitive.type != cgltf_primitive_type_triangles)
        {
            return;
        }

        const cgltf_accessor* positionAccessor = nullptr;
        for (cgltf_size attrIndex = 0; attrIndex < primitive.attributes_count; ++attrIndex)
        {
            const cgltf_attribute& attr = primitive.attributes[attrIndex];
            if (attr.type == cgltf_attribute_type_position)
            {
                positionAccessor = attr.data;
                break;
            }
        }

        if (!positionAccessor || positionAccessor->count == 0)
        {
            return;
        }

        const size_t baseVertex = mesh.positions.size();
        mesh.positions.reserve(baseVertex + positionAccessor->count);

        for (cgltf_size i = 0; i < positionAccessor->count; ++i)
        {
            float values[3] = {0.0f, 0.0f, 0.0f};
            cgltf_accessor_read_float(positionAccessor, i, values, 3);
            if (matrix)
            {
                float transformed[3];
                TransformPosition(matrix, values, transformed);
                mesh.positions.push_back({transformed[0], transformed[1], transformed[2]});
            }
            else
            {
                mesh.positions.push_back({values[0], values[1], values[2]});
            }
        }

        if (primitive.indices)
        {
            mesh.indices.reserve(mesh.indices.size() + primitive.indices->count);
            for (cgltf_size i = 0; i < primitive.indices->count; ++i)
            {
                const cgltf_size idx = cgltf_accessor_read_index(primitive.indices, i);
                mesh.indices.push_back(static_cast<std::uint32_t>(baseVertex + idx));
            }
        }
        else
        {
            mesh.indices.reserve(mesh.indices.size() + positionAccessor->count);
            for (cgltf_size i = 0; i < positionAccessor->count; ++i)
            {
                mesh.indices.push_back(static_cast<std::uint32_t>(baseVertex + i));
            }
        }
    };

    auto appendMesh = [&](const cgltf_mesh* srcMesh, const cgltf_float* matrix) {
        if (!srcMesh)
        {
            return;
        }
        for (cgltf_size primIndex = 0; primIndex < srcMesh->primitives_count; ++primIndex)
        {
            appendPrimitive(srcMesh->primitives[primIndex], matrix);
        }
    };

    auto traverseNode = [&](const cgltf_node* node, auto&& self) -> void {
        if (!node)
        {
            return;
        }

        cgltf_float matrix[16];
        cgltf_node_transform_world(node, matrix);
        appendMesh(node->mesh, matrix);

        for (cgltf_size i = 0; i < node->children_count; ++i)
        {
            self(node->children[i], self);
        }
    };

    bool usedNodes = false;
    if (data->scene && data->scene->nodes_count > 0)
    {
        usedNodes = true;
        for (cgltf_size i = 0; i < data->scene->nodes_count; ++i)
        {
            traverseNode(data->scene->nodes[i], traverseNode);
        }
    }
    else if (data->nodes_count > 0)
    {
        usedNodes = true;
        for (cgltf_size i = 0; i < data->nodes_count; ++i)
        {
            traverseNode(&data->nodes[i], traverseNode);
        }
    }

    if (!usedNodes)
    {
        for (cgltf_size meshIndex = 0; meshIndex < data->meshes_count; ++meshIndex)
        {
            appendMesh(&data->meshes[meshIndex], nullptr);
        }
    }

    cgltf_free(data);

    if (mesh.positions.empty() || mesh.indices.empty())
    {
        return nullptr;
    }

    auto& stored = m_meshData[assetId];
    stored = std::move(mesh);
    return &stored;
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
