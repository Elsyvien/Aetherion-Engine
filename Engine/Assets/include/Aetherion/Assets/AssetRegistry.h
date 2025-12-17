#pragma once

#include <string>
#include <unordered_map>

namespace Aetherion::Assets
{
class AssetRegistry
{
public:
    AssetRegistry() = default;
    ~AssetRegistry() = default;

    void Scan(const std::string& rootPath);
    [[nodiscard]] bool HasAsset(const std::string& assetId) const;

    // TODO: Replace string identifiers with strong asset handles/UUIDs.
    // TODO: Add import pipeline hooks and metadata caching.
private:
    std::unordered_map<std::string, std::string> m_placeholderAssets;
};
} // namespace Aetherion::Assets
