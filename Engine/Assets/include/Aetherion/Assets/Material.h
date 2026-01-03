#pragma once

#include <array>
#include <string>
#include <vector>

namespace Aetherion::Assets {
class Material {
public:
  Material() = default;
  ~Material() = default;

  // PBR Properties
  [[nodiscard]] std::array<float, 4> GetBaseColor() const {
    return m_baseColor;
  }
  void SetBaseColor(const std::array<float, 4> &color) { m_baseColor = color; }

  [[nodiscard]] float GetMetallic() const { return m_metallic; }
  void SetMetallic(float metallic) { m_metallic = metallic; }

  [[nodiscard]] float GetRoughness() const { return m_roughness; }
  void SetRoughness(float roughness) { m_roughness = roughness; }

  [[nodiscard]] std::array<float, 3> GetEmissiveFactor() const {
    return m_emissiveFactor;
  }
  void SetEmissiveFactor(const std::array<float, 3> &factor) {
    m_emissiveFactor = factor;
  }

  // Texture Asset IDs
  [[nodiscard]] const std::string &GetAlbedoMapId() const {
    return m_albedoMapId;
  }
  void SetAlbedoMapId(const std::string &id) { m_albedoMapId = id; }

  [[nodiscard]] const std::string &GetNormalMapId() const {
    return m_normalMapId;
  }
  void SetNormalMapId(const std::string &id) { m_normalMapId = id; }

  [[nodiscard]] const std::string &GetMetallicRoughnessMapId() const {
    return m_metallicRoughnessMapId;
  }
  void SetMetallicRoughnessMapId(const std::string &id) {
    m_metallicRoughnessMapId = id;
  }

  [[nodiscard]] const std::string &GetEmissiveMapId() const {
    return m_emissiveMapId;
  }
  void SetEmissiveMapId(const std::string &id) { m_emissiveMapId = id; }

  [[nodiscard]] const std::string &GetOcclusionMapId() const {
    return m_occlusionMapId;
  }
  void SetOcclusionMapId(const std::string &id) { m_occlusionMapId = id; }

  // Constants
  static constexpr std::string_view kExtension = ".mat";

private:
  std::array<float, 4> m_baseColor{1.0f, 1.0f, 1.0f, 1.0f};
  float m_metallic{0.0f};
  float m_roughness{0.5f};
  std::array<float, 3> m_emissiveFactor{0.0f, 0.0f, 0.0f};

  std::string m_albedoMapId;
  std::string m_normalMapId;
  std::string m_metallicRoughnessMapId;
  std::string m_emissiveMapId;
  std::string m_occlusionMapId;
};
} // namespace Aetherion::Assets
