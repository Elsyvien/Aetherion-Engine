#pragma once

#include "Aetherion/Scene/Component.h"
#include <vector>
#include <string>
#include <map>

namespace Aetherion::Scene
{

class SemanticComponent : public Component
{
public:
    [[nodiscard]] std::string GetDisplayName() const override { return "Semantic"; }

    // Semantic Tags (e.g., "Wood", "Flammable", "Furniture")
    void AddTag(const std::string& tag);
    void RemoveTag(const std::string& tag);
    [[nodiscard]] bool HasTag(const std::string& tag) const;
    [[nodiscard]] const std::vector<std::string>& GetTags() const { return m_tags; }

    // Natural Language Description (e.g., "A worn wooden chair with a missing leg")
    void SetDescription(const std::string& description) { m_description = description; }
    [[nodiscard]] const std::string& GetDescription() const { return m_description; }

    // Semantic Attributes (e.g., "DangerLevel": 0.8, "Value": 50.0)
    void SetAttribute(const std::string& key, float value);
    [[nodiscard]] float GetAttribute(const std::string& key, float defaultValue = 0.0f) const;
    [[nodiscard]] const std::map<std::string, float>& GetAttributes() const { return m_attributes; }

protected:
    void OnBeginPlay() override;

private:
    std::vector<std::string> m_tags;
    std::string m_description;
    std::map<std::string, float> m_attributes;
};

} // namespace Aetherion::Scene
