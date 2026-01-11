#include "Aetherion/Scripting/PythonBindings.h"

#include "Aetherion/Core/Types.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/TransformComponent.h"

#include <array>

#ifdef AETHERION_ENABLE_PYBIND11
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;
#endif

namespace Aetherion::Scripting {

#ifdef AETHERION_ENABLE_PYTHON

#ifdef AETHERION_ENABLE_PYBIND11
class EntityProxy {
public:
    explicit EntityProxy(Scene::Entity* entity) : m_entity(entity) {}

    [[nodiscard]] std::uint64_t GetId() const {
        return m_entity ? static_cast<std::uint64_t>(m_entity->GetId()) : 0;
    }

    [[nodiscard]] std::string GetName() const {
        return m_entity ? m_entity->GetName() : std::string();
    }

    void SetName(const std::string& name) {
        if (m_entity) {
            m_entity->SetName(name);
        }
    }

    [[nodiscard]] std::array<float, 3> GetPosition() const {
        if (!m_entity) {
            return {0.0f, 0.0f, 0.0f};
        }
        auto transform = m_entity->GetComponent<Scene::TransformComponent>();
        if (!transform) {
            return {0.0f, 0.0f, 0.0f};
        }
        return {transform->GetPositionX(), transform->GetPositionY(),
                transform->GetPositionZ()};
    }

    void SetPosition(float x, float y, float z) {
        if (!m_entity) {
            return;
        }
        auto transform = m_entity->GetComponent<Scene::TransformComponent>();
        if (!transform) {
            return;
        }
        transform->SetPosition(x, y, z);
    }

    [[nodiscard]] std::array<float, 3> GetRotation() const {
        if (!m_entity) {
            return {0.0f, 0.0f, 0.0f};
        }
        auto transform = m_entity->GetComponent<Scene::TransformComponent>();
        if (!transform) {
            return {0.0f, 0.0f, 0.0f};
        }
        return {transform->GetRotationXDegrees(),
                transform->GetRotationYDegrees(),
                transform->GetRotationZDegrees()};
    }

    void SetRotation(float x, float y, float z) {
        if (!m_entity) {
            return;
        }
        auto transform = m_entity->GetComponent<Scene::TransformComponent>();
        if (!transform) {
            return;
        }
        transform->SetRotationDegrees(x, y, z);
    }

    [[nodiscard]] std::array<float, 3> GetScale() const {
        if (!m_entity) {
            return {1.0f, 1.0f, 1.0f};
        }
        auto transform = m_entity->GetComponent<Scene::TransformComponent>();
        if (!transform) {
            return {1.0f, 1.0f, 1.0f};
        }
        return {transform->GetScaleX(), transform->GetScaleY(),
                transform->GetScaleZ()};
    }

    void SetScale(float x, float y, float z) {
        if (!m_entity) {
            return;
        }
        auto transform = m_entity->GetComponent<Scene::TransformComponent>();
        if (!transform) {
            return;
        }
        transform->SetScale(x, y, z);
    }

private:
    Scene::Entity* m_entity{nullptr};
};

PYBIND11_EMBEDDED_MODULE(aetherion, m) {
    py::class_<EntityProxy>(m, "Entity")
        .def("id", &EntityProxy::GetId)
        .def("name", &EntityProxy::GetName)
        .def("set_name", &EntityProxy::SetName)
        .def("get_position", &EntityProxy::GetPosition)
        .def("set_position", &EntityProxy::SetPosition)
        .def("get_rotation", &EntityProxy::GetRotation)
        .def("set_rotation", &EntityProxy::SetRotation)
        .def("get_scale", &EntityProxy::GetScale)
        .def("set_scale", &EntityProxy::SetScale);
}
#endif

void InitializePythonBindings() {
#ifdef AETHERION_ENABLE_PYBIND11
    if (!Py_IsInitialized()) {
        return;
    }
    PyObject* module = PyImport_ImportModule("aetherion");
    if (!module) {
        PyErr_Clear();
        return;
    }
    Py_DECREF(module);
#endif
}

PyObject* CreateEntityProxy(Scene::Entity* entity) {
#ifdef AETHERION_ENABLE_PYBIND11
    if (!entity) {
        Py_INCREF(Py_None);
        return Py_None;
    }
    py::object obj = py::cast(EntityProxy(entity));
    return obj.release().ptr();
#else
    Py_INCREF(Py_None);
    return Py_None;
#endif
}

#else

void InitializePythonBindings() {}

PyObject* CreateEntityProxy(Scene::Entity* entity) {
    (void)entity;
    return nullptr;
}

#endif

} // namespace Aetherion::Scripting
