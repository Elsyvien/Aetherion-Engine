#pragma once

#include "Aetherion/Scene/Component.h"
#include <string>

namespace Aetherion::Scene {

class ScriptComponent final : public Component {
public:
    ScriptComponent();
    ~ScriptComponent() override = default;

    [[nodiscard]] std::string GetDisplayName() const override { return "Script"; }

    void SetScriptAssetId(const std::string& assetId) { m_scriptAssetId = assetId; }
    [[nodiscard]] const std::string& GetScriptAssetId() const { return m_scriptAssetId; }

    void SetScriptSource(const std::string& source) { m_scriptSource = source; }
    [[nodiscard]] const std::string& GetScriptSource() const { return m_scriptSource; }

protected:
    void OnBeginPlay() override;
    void OnEndPlay() override;
    void OnUpdate(float deltaTime) override;

private:
    std::string m_scriptAssetId;
    std::string m_scriptSource;
};

} // namespace Aetherion::Scene
