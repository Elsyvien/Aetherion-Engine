#include "Aetherion/Assets/AssetRegistry.h"

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
    return m_placeholderAssets.find(assetId) != m_placeholderAssets.end();
}
} // namespace Aetherion::Assets
