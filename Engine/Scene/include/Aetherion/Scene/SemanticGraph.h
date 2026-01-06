#pragma once

#include <vector>
#include <string>

namespace Aetherion::Scene
{
    class Scene;
    class Entity;

    class SemanticGraph
    {
    public:
        explicit SemanticGraph(Scene* scene);

        // Core API
        [[nodiscard]] std::vector<Entity*> FindEntitiesWithTag(const std::string& tag) const;
        [[nodiscard]] std::vector<Entity*> FindEntitiesWithAttribute(const std::string& key, float minValue = -1e9, float maxValue = 1e9) const;
        
        // Advanced Semantic Queries (Groundwork for AI)
        [[nodiscard]] std::vector<Entity*> FindContextuallyRelevantEntities(const std::string& contextDescription) const;

        // Triggers an update of the semantic index (could be async in future)
        void RebuildIndex();

    private:
        Scene* m_scene;
    };
}
