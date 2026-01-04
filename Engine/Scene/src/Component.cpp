#include "Aetherion/Scene/Component.h"

namespace Aetherion::Scene
{
void Component::Bind(Entity* entity, Scene* scene)
{
    const bool wasBound = (m_entity != nullptr);
    m_entity = entity;
    m_scene = scene;
    if (!wasBound && m_entity)
    {
        OnAdded();
    }
}

void Component::Unbind()
{
    if (m_started)
    {
        EndPlay();
    }
    OnRemoved();
    m_scene = nullptr;
    m_entity = nullptr;
}

void Component::BeginPlay()
{
    if (m_started)
    {
        return;
    }
    m_started = true;
    OnBeginPlay();
}

void Component::EndPlay()
{
    if (!m_started)
    {
        return;
    }
    OnEndPlay();
    m_started = false;
}

void Component::Update(float deltaTime)
{
    if (!m_started)
    {
        return;
    }
    OnUpdate(deltaTime);
}
} // namespace Aetherion::Scene
