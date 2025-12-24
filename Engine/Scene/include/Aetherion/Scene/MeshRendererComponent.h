#pragma once

#include <array>
#include <string>
#include <utility>

#include "Aetherion/Scene/Component.h"

namespace Aetherion::Scene
{
class MeshRendererComponent final : public Component
{
public:
    MeshRendererComponent();
    ~MeshRendererComponent() override = default;

    [[nodiscard]] std::string GetDisplayName() const override;

    [[nodiscard]] bool IsVisible() const noexcept { return m_visible; }
    void SetVisible(bool visible) noexcept { m_visible = visible; }

    [[nodiscard]] std::array<float, 3> GetColor() const noexcept { return m_color; }
    void SetColor(float r, float g, float b) noexcept;

    [[nodiscard]] float GetRotationSpeedDegPerSec() const noexcept { return m_rotationSpeedDegPerSec; }
    void SetRotationSpeedDegPerSec(float speed) noexcept;

    [[nodiscard]] const std::string& GetMeshAssetId() const noexcept { return m_meshAssetId; }
    void SetMeshAssetId(std::string assetId) { m_meshAssetId = std::move(assetId); }

    [[nodiscard]] const std::string& GetAlbedoTextureId() const noexcept { return m_albedoTextureId; }
    void SetAlbedoTextureId(std::string assetId) { m_albedoTextureId = std::move(assetId); }

private:
    bool m_visible{true};
    std::array<float, 3> m_color{1.0f, 1.0f, 1.0f};
    float m_rotationSpeedDegPerSec{0.0f};
    std::string m_meshAssetId;
    std::string m_albedoTextureId;
};
} // namespace Aetherion::Scene
