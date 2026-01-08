#include "Aetherion/Scripting/LuaBindings.h"
#include "Aetherion/Scene/AudioSourceComponent.h"
#include "Aetherion/Scene/CameraComponent.h"
#include "Aetherion/Scene/ColliderComponent.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/LightComponent.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/ParticleEmitterComponent.h"
#include "Aetherion/Scene/RigidbodyComponent.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/TransformComponent.h"

#ifdef AETHERION_ENABLE_LUA
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#endif

#include <iostream>

namespace Aetherion::Scripting {

// Simple Vec3 struct for Lua binding
struct Vec3 {
  float x, y, z;
};

void LuaBindings::RegisterAll(sol::state &lua, Scene::Scene *scene) {
#ifdef AETHERION_ENABLE_LUA
  RegisterMath(lua);
  RegisterLogging(lua);
  RegisterTime(lua);
  RegisterComponents(lua);
  RegisterEntity(lua, scene);
  RegisterSceneQueries(lua, scene);
#else
  (void)lua;
  (void)scene;
#endif
}

void LuaBindings::RegisterMath(sol::state &lua) {
#ifdef AETHERION_ENABLE_LUA
  // Vec3 type
  lua.new_usertype<struct Vec3>(
      "Vec3", sol::constructors<Vec3(), Vec3(float, float, float)>(), "x",
      sol::property([](Vec3 &v) { return v.x; },
                    [](Vec3 &v, float val) { v.x = val; }),
      "y",
      sol::property([](Vec3 &v) { return v.y; },
                    [](Vec3 &v, float val) { v.y = val; }),
      "z",
      sol::property([](Vec3 &v) { return v.z; },
                    [](Vec3 &v, float val) { v.z = val; }),
      "length",
      [](const Vec3 &v) {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
      },
      "normalize",
      [](Vec3 &v) {
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len > 0.0001f) {
          v.x /= len;
          v.y /= len;
          v.z /= len;
        }
      },
      sol::meta_function::addition,
      [](const Vec3 &a, const Vec3 &b) {
        return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
      },
      sol::meta_function::subtraction,
      [](const Vec3 &a, const Vec3 &b) {
        return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
      },
      sol::meta_function::multiplication,
      [](const Vec3 &v, float s) { return Vec3{v.x * s, v.y * s, v.z * s}; });

  // Create Vec3 constructor helper
  lua["Vec3"] = [](float x, float y, float z) { return Vec3{x, y, z}; };

  // Math utilities
  lua["aetherion"]["math"] = lua.create_table();
  lua["aetherion"]["math"]["lerp"] = [](float a, float b, float t) {
    return a + (b - a) * t;
  };
  lua["aetherion"]["math"]["clamp"] = [](float v, float min, float max) {
    return v < min ? min : (v > max ? max : v);
  };
  lua["aetherion"]["math"]["radians"] = [](float deg) {
    return deg * 3.14159265f / 180.0f;
  };
  lua["aetherion"]["math"]["degrees"] = [](float rad) {
    return rad * 180.0f / 3.14159265f;
  };
#else
  (void)lua;
#endif
}

void LuaBindings::RegisterLogging(sol::state &lua) {
#ifdef AETHERION_ENABLE_LUA
  lua["print"] = [](sol::variadic_args va) {
    for (auto v : va) {
      if (v.is<std::string>()) {
        std::cout << v.as<std::string>();
      } else if (v.is<double>()) {
        std::cout << v.as<double>();
      } else if (v.is<bool>()) {
        std::cout << (v.as<bool>() ? "true" : "false");
      } else {
        std::cout << "<lua object>";
      }
      std::cout << "\t";
    }
    std::cout << std::endl;
  };

  lua["aetherion"]["log"] = lua.create_table();
  lua["aetherion"]["log"]["info"] = [](const std::string &msg) {
    std::cout << "[Lua INFO] " << msg << std::endl;
  };
  lua["aetherion"]["log"]["warn"] = [](const std::string &msg) {
    std::cerr << "[Lua WARN] " << msg << std::endl;
  };
  lua["aetherion"]["log"]["error"] = [](const std::string &msg) {
    std::cerr << "[Lua ERROR] " << msg << std::endl;
  };
#else
  (void)lua;
#endif
}

void LuaBindings::RegisterTime(sol::state &lua) {
#ifdef AETHERION_ENABLE_LUA
  // deltaTime is set by the engine each frame
  // We also provide a time table for utilities
  lua["aetherion"]["time"] = lua.create_table();
  lua["aetherion"]["time"]["delta"] = [&lua]() {
    return lua["aetherion"]["deltaTime"].get_or(0.0f);
  };
#else
  (void)lua;
#endif
}

void LuaBindings::RegisterComponents(sol::state &lua) {
#ifdef AETHERION_ENABLE_LUA
  // TransformComponent binding
  lua.new_usertype<Scene::TransformComponent>(
      "Transform", sol::no_constructor, "getPosition",
      [](Scene::TransformComponent &t) {
        return Vec3{t.GetPositionX(), t.GetPositionY(), t.GetPositionZ()};
      },
      "setPosition",
      sol::overload([](Scene::TransformComponent &t, float x, float y,
                       float z) { t.SetPosition(x, y, z); },
                    [](Scene::TransformComponent &t, const Vec3 &v) {
                      t.SetPosition(v.x, v.y, v.z);
                    }),
      "getScale",
      [](Scene::TransformComponent &t) {
        return Vec3{t.GetScaleX(), t.GetScaleY(), t.GetScaleZ()};
      },
      "setScale",
      sol::overload(
          [](Scene::TransformComponent &t, float x, float y, float z) {
            t.SetScale(x, y, z);
          },
          [](Scene::TransformComponent &t, float s) { t.SetScale(s, s, s); }),
      "getRotation",
      [](Scene::TransformComponent &t) {
        return Vec3{t.GetRotationX(), t.GetRotationY(), t.GetRotationZ()};
      },
      "setRotation",
      [](Scene::TransformComponent &t, float x, float y, float z) {
        t.SetRotation(x, y, z);
      },
      "translate",
      [](Scene::TransformComponent &t, float dx, float dy, float dz) {
        t.SetPosition(t.GetPositionX() + dx, t.GetPositionY() + dy,
                      t.GetPositionZ() + dz);
      },
      "rotate",
      [](Scene::TransformComponent &t, float dx, float dy, float dz) {
        t.SetRotation(t.GetRotationX() + dx, t.GetRotationY() + dy,
                      t.GetRotationZ() + dz);
      });

  // MeshRendererComponent binding
  lua.new_usertype<Scene::MeshRendererComponent>(
      "MeshRenderer", sol::no_constructor, "getMesh",
      &Scene::MeshRendererComponent::GetMeshAssetId, "setMesh",
      &Scene::MeshRendererComponent::SetMeshAssetId, "setColor",
      &Scene::MeshRendererComponent::SetColor, "setRotationSpeed",
      &Scene::MeshRendererComponent::SetRotationSpeedDegPerSec);

  // LightComponent binding
  lua.new_usertype<Scene::LightComponent>(
      "Light", sol::no_constructor, "getIntensity",
      &Scene::LightComponent::GetIntensity, "setIntensity",
      &Scene::LightComponent::SetIntensity, "setColor",
      &Scene::LightComponent::SetColor);
#else
  (void)lua;
#endif
}

void LuaBindings::RegisterEntity(sol::state &lua, Scene::Scene *scene) {
#ifdef AETHERION_ENABLE_LUA
  lua.new_usertype<Scene::Entity>(
      "Entity", sol::no_constructor, "getId", &Scene::Entity::GetId, "getName",
      &Scene::Entity::GetName, "setName", &Scene::Entity::SetName,
      "getTransform",
      [](Scene::Entity &e) {
        return e.GetComponent<Scene::TransformComponent>();
      },
      "getMeshRenderer",
      [](Scene::Entity &e) {
        return e.GetComponent<Scene::MeshRendererComponent>();
      },
      "getLight",
      [](Scene::Entity &e) { return e.GetComponent<Scene::LightComponent>(); },
      "getCamera",
      [](Scene::Entity &e) { return e.GetComponent<Scene::CameraComponent>(); },
      "getRigidbody",
      [](Scene::Entity &e) {
        return e.GetComponent<Scene::RigidbodyComponent>();
      },
      "getCollider",
      [](Scene::Entity &e) {
        return e.GetComponent<Scene::ColliderComponent>();
      },
      "getAudioSource",
      [](Scene::Entity &e) {
        return e.GetComponent<Scene::AudioSourceComponent>();
      });

  // Aetherion entity access functions
  if (scene) {
    lua["aetherion"]["getEntity"] = [scene](uint64_t id) -> Scene::Entity * {
      auto entity = scene->GetEntityById(static_cast<Core::EntityId>(id));
      return entity.get();
    };

    lua["aetherion"]["findEntity"] =
        [scene](const std::string &name) -> Scene::Entity * {
      for (const auto &e : scene->GetEntities()) {
        if (e && e->GetName() == name) {
          return e.get();
        }
      }
      return nullptr;
    };
  }
#else
  (void)lua;
  (void)scene;
#endif
}

void LuaBindings::RegisterSceneQueries(sol::state &lua, Scene::Scene *scene) {
#ifdef AETHERION_ENABLE_LUA
  if (!scene)
    return;

  lua["aetherion"]["scene"] = lua.create_table();

  lua["aetherion"]["scene"]["getAllEntities"] = [scene]() {
    std::vector<Scene::Entity *> result;
    for (const auto &e : scene->GetEntities()) {
      if (e)
        result.push_back(e.get());
    }
    return result;
  };

  lua["aetherion"]["scene"]["getEntityCount"] = [scene]() {
    return scene->GetEntities().size();
  };

  lua["aetherion"]["scene"]["getName"] = [scene]() { return scene->GetName(); };
#else
  (void)lua;
  (void)scene;
#endif
}

} // namespace Aetherion::Scripting
