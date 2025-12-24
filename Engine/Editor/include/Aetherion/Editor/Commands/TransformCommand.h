#pragma once

#include "Aetherion/Editor/Command.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/TransformComponent.h"
#include "Aetherion/Core/Types.h"
#include <memory>
#include <array>

namespace Aetherion::Editor
{
struct TransformData
{
    std::array<float, 3> position;
    std::array<float, 3> rotation;
    std::array<float, 3> scale;
    
    bool operator==(const TransformData& other) const
    {
        return position == other.position && rotation == other.rotation && scale == other.scale;
    }
};

class TransformCommand : public Command
{
public:
    TransformCommand(std::shared_ptr<Scene::Entity> entity, const TransformData& oldTrans, const TransformData& newTrans)
        : m_entity(std::move(entity)), m_old(oldTrans), m_new(newTrans)
    {
    }

    void Do() override
    {
        Apply(m_new);
    }

    void Undo() override
    {
        Apply(m_old);
    }
    
    bool Merge(const Command* other) override
    {
        const auto* typedOther = dynamic_cast<const TransformCommand*>(other);
        if (!typedOther) return false;
        
        if (m_entity != typedOther->m_entity) return false;
        
        // Update new state to the other's new state
        m_new = typedOther->m_new;
        return true;
    }

    [[nodiscard]] std::string GetName() const override { return "Transform Change"; }

private:
    void Apply(const TransformData& data)
    {
        if (!m_entity) return;
        auto transform = m_entity->GetComponent<Scene::TransformComponent>();
        if (!transform) return;

        transform->SetPosition(data.position[0], data.position[1], data.position[2]);
        transform->SetRotationDegrees(data.rotation[0], data.rotation[1], data.rotation[2]);
        transform->SetScale(data.scale[0], data.scale[1], data.scale[2]);
    }

    std::shared_ptr<Scene::Entity> m_entity;
    TransformData m_old;
    TransformData m_new;
};
} // namespace Aetherion::Editor
