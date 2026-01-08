#include "Aetherion/Scene/AIBehaviorSystem.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/AIBehaviorComponent.h"
#include "Aetherion/Scene/TransformComponent.h"
#include <iostream>
#include <random>

namespace Aetherion::Scene
{

void AIBehaviorSystem::Configure(Runtime::EngineContext& context)
{
    // Register dependencies or services if needed
    (void)context;
}

void AIBehaviorSystem::Update(Scene& scene, float deltaTime)
{
    const auto& entities = scene.GetEntities();
    for (const auto& entity : entities)
    {
        if (entity && entity->GetComponent<AIBehaviorComponent>())
        {
            ProcessEntity(scene, entity.get(), deltaTime);
        }
    }
}

void AIBehaviorSystem::ProcessEntity(Scene& scene, Entity* entity, float deltaTime)
{
    auto aiComp = entity->GetComponent<AIBehaviorComponent>();
    if (!aiComp) return;

    aiComp->m_timeSinceLastThought += deltaTime;

    if (aiComp->m_timeSinceLastThought >= aiComp->GetDecisionInterval())
    {
        aiComp->m_timeSinceLastThought = 0.0f;
        
        // --- Simulate AI "Thinking" ---
        // In a real implementation, this would queue a request to the LLM/Inference Engine
        // passing the personality and scene context.
        
        // Simple random behavior for prototype
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> actionDist(0, 3);
        
        int action = actionDist(gen);
        std::string personality = aiComp->GetPersonality();
        std::string entityName = entity->GetName();
        
        if (action == 0)
        {
            aiComp->SetCurrentState("Idle");
            std::cout << "[AI] " << entityName << " (" << personality << "): Decided to wait and observe." << std::endl;
        }
        else if (action == 1)
        {
            aiComp->SetCurrentState("Moving");
            auto transform = entity->GetComponent<TransformComponent>();
            if (transform)
            {
                // Jiggle position slightly
                float dx = (static_cast<float>(actionDist(gen)) - 1.5f) * 0.5f;
                float dz = (static_cast<float>(actionDist(gen)) - 1.5f) * 0.5f;
                
                float x = transform->GetPositionX() + dx;
                float z = transform->GetPositionZ() + dz;
                transform->SetPosition(x, transform->GetPositionY(), z);
                
                std::cout << "[AI] " << entityName << " (" << personality << "): Moving to (" << x << ", " << z << ")." << std::endl;
            }
        }
        else if (action == 2)
        {
             aiComp->SetCurrentState("Speaking");
             std::cout << "[AI] " << entityName << " (" << personality << "): 'Hello world! I am aware.'" << std::endl;
        }
    }
}

} // namespace Aetherion::Scene
