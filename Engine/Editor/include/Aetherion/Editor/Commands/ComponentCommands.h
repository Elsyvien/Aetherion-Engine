#pragma once

#include "Aetherion/Editor/Command.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Component.h"
#include <memory>
#include <string>

namespace Aetherion::Editor
{
class AddComponentCommand : public Command
{
public:
    AddComponentCommand(std::shared_ptr<Scene::Entity> entity, std::shared_ptr<Scene::Component> component)
        : m_entity(std::move(entity)), m_component(std::move(component))
    {
    }

    void Do() override
    {
        if (m_entity && m_component)
        {
            m_entity->AddComponent(m_component);
        }
    }

    void Undo() override
    {
        if (m_entity && m_component)
        {
            m_entity->RemoveComponent(m_component);
        }
    }

    [[nodiscard]] std::string GetName() const override { return "Add Component"; }

private:
    std::shared_ptr<Scene::Entity> m_entity;
    std::shared_ptr<Scene::Component> m_component;
};

class RemoveComponentCommand : public Command
{
public:
    RemoveComponentCommand(std::shared_ptr<Scene::Entity> entity, std::shared_ptr<Scene::Component> component)
        : m_entity(std::move(entity)), m_component(std::move(component))
    {
    }

    void Do() override
    {
        if (m_entity && m_component)
        {
            m_entity->RemoveComponent(m_component);
        }
    }

    void Undo() override
    {
        if (m_entity && m_component)
        {
            m_entity->AddComponent(m_component);
        }
    }

    [[nodiscard]] std::string GetName() const override { return "Remove Component"; }

private:
    std::shared_ptr<Scene::Entity> m_entity;
    std::shared_ptr<Scene::Component> m_component;
};
} // namespace Aetherion::Editor
