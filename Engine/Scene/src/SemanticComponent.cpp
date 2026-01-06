#include "Aetherion/Scene/SemanticComponent.h"
#include <algorithm>

namespace Aetherion::Scene
{

void SemanticComponent::AddTag(const std::string& tag)
{
    if (std::find(m_tags.begin(), m_tags.end(), tag) == m_tags.end())
    {
        m_tags.push_back(tag);
    }
}

void SemanticComponent::RemoveTag(const std::string& tag)
{
    auto it = std::remove(m_tags.begin(), m_tags.end(), tag);
    if (it != m_tags.end())
    {
        m_tags.erase(it, m_tags.end());
    }
}

bool SemanticComponent::HasTag(const std::string& tag) const
{
    return std::find(m_tags.begin(), m_tags.end(), tag) != m_tags.end();
}

void SemanticComponent::SetAttribute(const std::string& key, float value)
{
    m_attributes[key] = value;
}

float SemanticComponent::GetAttribute(const std::string& key, float defaultValue) const
{
    auto it = m_attributes.find(key);
    if (it != m_attributes.end())
    {
        return it->second;
    }
    return defaultValue;
}

void SemanticComponent::OnBeginPlay()
{
    // potentially register with the SemanticGraph here if we had global access
}

} // namespace Aetherion::Scene
