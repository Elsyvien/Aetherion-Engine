#include "Aetherion/Scene/SemanticGraph.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/SemanticComponent.h"

namespace Aetherion::Scene
{
    SemanticGraph::SemanticGraph(Scene* scene)
        : m_scene(scene)
    {
    }

    std::vector<Entity*> SemanticGraph::FindEntitiesWithTag(const std::string& tag) const
    {
        std::vector<Entity*> result;
        if (!m_scene) return result;

        for (const auto& entity : m_scene->GetEntities())
        {
            if (auto comp = entity->GetComponent<SemanticComponent>())
            {
                if (comp->HasTag(tag))
                {
                    result.push_back(entity.get());
                }
            }
        }
        return result;
    }

    std::vector<Entity*> SemanticGraph::FindEntitiesWithAttribute(const std::string& key, float minValue, float maxValue) const
    {
        std::vector<Entity*> result;
        if (!m_scene) return result;

        for (const auto& entity : m_scene->GetEntities())
        {
            if (auto comp = entity->GetComponent<SemanticComponent>())
            {
                float val = comp->GetAttribute(key, -std::numeric_limits<float>::infinity());
                // Check if attribute exists (simple check: if it returns default, maybe we should check map directly? 
                // but Component API allows default. Let's assume if it returns a value in range, it counts.)
                
                // Better approach: check map keys directly if possible, but API hides it.
                // For now, relies on value check.
                 const auto& attrs = comp->GetAttributes();
                 if (attrs.find(key) != attrs.end())
                 {
                     if (val >= minValue && val <= maxValue)
                     {
                         result.push_back(entity.get());
                     }
                 }
            }
        }
        return result;
    }

    std::vector<Entity*> SemanticGraph::FindContextuallyRelevantEntities(const std::string& contextDescription) const
    {
        // This is where the AI logic would go.
        // For now, we'll do a simple string containment search on description and tags.
        std::vector<Entity*> result;
        if (!m_scene) return result;

        for (const auto& entity : m_scene->GetEntities())
        {
            if (auto comp = entity->GetComponent<SemanticComponent>())
            {
                bool match = false;
                if (comp->GetDescription().find(contextDescription) != std::string::npos) match = true;
                
                for (const auto& tag : comp->GetTags())
                {
                    if (tag.find(contextDescription) != std::string::npos) match = true;
                }

                if (match)
                {
                    result.push_back(entity.get());
                }
            }
        }
        return result;
    }

    void SemanticGraph::RebuildIndex()
    {
        // No-op for now as we iterate linearly.
        // In future, build an acceleration structure (e.g., inverted index for tags).
    }
}
