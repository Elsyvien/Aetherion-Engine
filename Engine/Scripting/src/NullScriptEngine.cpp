#include "Aetherion/Scripting/ScriptEngine.h"
#include "Aetherion/Scripting/ScriptInstance.h"

namespace Aetherion::Scripting {

class NullScriptInstance : public ScriptInstance {
public:
    void SetEntity(Scene::Entity* entity) override { (void)entity; }
    void OnCreate() override {}
    void OnUpdate(float deltaTime) override { (void)deltaTime; }
    void OnDestroy() override {}
};

class NullScriptEngine : public ScriptEngine {
public:
    void Initialize() override {}
    void Shutdown() override {}

    std::unique_ptr<ScriptInstance>
    CreateInstance(const std::string& scriptSource, SourceKind sourceKind) override {
        (void)scriptSource;
        (void)sourceKind;
        return std::make_unique<NullScriptInstance>();
    }

    void OnUpdate(float deltaTime) override { (void)deltaTime; }
};

} // namespace Aetherion::Scripting
