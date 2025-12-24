#pragma once

#include "Aetherion/Editor/Command.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"
#include <memory>
#include <string>

namespace Aetherion::Editor
{
class RenameEntityCommand : public Command
{
public:
    RenameEntityCommand(std::shared_ptr<Scene::Entity> entity, const std::string& oldName, const std::string& newName)
        : m_entity(std::move(entity)), m_oldName(oldName), m_newName(newName)
    {
    }

    void Do() override
    {
        if (m_entity) m_entity->SetName(m_newName);
    }

    void Undo() override
    {
        if (m_entity) m_entity->SetName(m_oldName);
    }

    [[nodiscard]] std::string GetName() const override { return "Rename Entity"; }

private:
    std::shared_ptr<Scene::Entity> m_entity;
    std::string m_oldName;
    std::string m_newName;
};

class DeleteEntityCommand : public Command
{
public:
    DeleteEntityCommand(std::shared_ptr<Scene::Scene> scene, std::shared_ptr<Scene::Entity> entity)
        : m_scene(std::move(scene)), m_entity(std::move(entity))
    {
    }

    void Do() override
    {
        if (m_scene && m_entity)
        {
            m_scene->RemoveEntity(m_entity->GetId());
        }
    }

    void Undo() override
    {
        if (m_scene && m_entity)
        {
            m_scene->AddEntity(m_entity);
        }
    }

    [[nodiscard]] std::string GetName() const override { return "Delete Entity"; }

private:
    std::shared_ptr<Scene::Scene> m_scene;
    std::shared_ptr<Scene::Entity> m_entity;
};

class CreateEntityCommand : public Command
{
public:
    CreateEntityCommand(std::shared_ptr<Scene::Scene> scene, std::shared_ptr<Scene::Entity> entity)
        : m_scene(std::move(scene)), m_entity(std::move(entity))
    {
    }

    void Do() override
    {
        if (m_scene && m_entity)
        {
            m_scene->AddEntity(m_entity);
        }
    }

    void Undo() override
    {
        if (m_scene && m_entity)
        {
            m_scene->RemoveEntity(m_entity->GetId());
        }
    }

    [[nodiscard]] std::string GetName() const override { return "Create Entity"; }

private:
    std::shared_ptr<Scene::Scene> m_scene;
    std::shared_ptr<Scene::Entity> m_entity;
};
} // namespace Aetherion::Editor
