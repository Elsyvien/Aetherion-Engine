#include "RotationBehavior.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"
#include <iostream>

namespace Aetherion::Generated
{

void RotationBehavior::OnAdded()
{
    std::cout << "RotationBehavior: OnAdded" << std::endl;
}

void RotationBehavior::OnUpdate(float deltaTime)
{
    auto transform = GetTransform();
    if (!transform)
        return;

    // Get current rotation
    auto [x, y, z] = transform->GetRotationDegrees();

    // Apply rotation
    float rotationDelta = m_rotationSpeed * deltaTime;
    y += rotationDelta;

    // Wrap around 360 degrees
    if (y >= 360.0f)
        y -= 360.0f;

    // Set new rotation
    transform->SetRotationDegrees(x, y, z);
}

void RotationBehavior::OnRemoved()
{
    std::cout << "RotationBehavior: OnRemoved" << std::endl;
}

// Module implementation
void RotationBehaviorModule::OnLoad(Scene::Scene* scene)
{
    m_scene = scene;
    std::cout << "RotationBehaviorModule: Loaded" << std::endl;
}

void RotationBehaviorModule::OnUnload()
{
    std::cout << "RotationBehaviorModule: Unloaded" << std::endl;
    m_scene = nullptr;
}

} // namespace Aetherion::Generated

// Module entry points
extern "C" {

AETHERION_EXPORT Aetherion::Scripting::IGameModule* CreateGameModule()
{
    return new Aetherion::Generated::RotationBehaviorModule();
}

AETHERION_EXPORT void DestroyGameModule(Aetherion::Scripting::IGameModule* module)
{
    delete module;
}

}
