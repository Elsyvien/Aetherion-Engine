#pragma once

#include <array>
#include <memory>
#include <string>

#include "Aetherion/Editor/Command.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/MeshRendererComponent.h"

namespace Aetherion::Editor
{
struct MeshRendererState
{
    std::array<float, 3> color{1.0f, 1.0f, 1.0f};
    float rotationSpeedDegPerSec{0.0f};
    bool visible{true};
    std::string meshAssetId;
    std::string materialAssetId;
};

inline MeshRendererState CaptureMeshRendererState(const Scene::MeshRendererComponent& mesh)
{
    MeshRendererState state;
    state.color = mesh.GetColor();
    state.rotationSpeedDegPerSec = mesh.GetRotationSpeedDegPerSec();
    state.visible = mesh.IsVisible();
    state.meshAssetId = mesh.GetMeshAssetId();
    state.materialAssetId = mesh.GetMaterialAssetId();
    return state;
}

class MeshRendererCommand : public Command
{
public:
    MeshRendererCommand(std::shared_ptr<Scene::Entity> entity,
                        const MeshRendererState& oldState,
                        const MeshRendererState& newState)
        : m_entity(std::move(entity)), m_old(oldState), m_new(newState)
    {
    }

    void Do() override { Apply(m_new); }
    void Undo() override { Apply(m_old); }

    [[nodiscard]] std::string GetName() const override { return "Mesh Renderer Change"; }

private:
    void Apply(const MeshRendererState& state)
    {
        if (!m_entity)
        {
            return;
        }
        auto mesh = m_entity->GetComponent<Scene::MeshRendererComponent>();
        if (!mesh)
        {
            return;
        }

        mesh->SetColor(state.color[0], state.color[1], state.color[2]);
        mesh->SetRotationSpeedDegPerSec(state.rotationSpeedDegPerSec);
        mesh->SetVisible(state.visible);
        mesh->SetMeshAssetId(state.meshAssetId);
        mesh->SetMaterialAssetId(state.materialAssetId);
    }

    std::shared_ptr<Scene::Entity> m_entity;
    MeshRendererState m_old;
    MeshRendererState m_new;
};
} // namespace Aetherion::Editor
