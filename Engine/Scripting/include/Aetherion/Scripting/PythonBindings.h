#pragma once

#include <cstdint>

#ifdef AETHERION_ENABLE_PYTHON
#include <Python.h>
#else
struct _object;
using PyObject = _object;
#endif

namespace Aetherion::Scene {
class Entity;
}

namespace Aetherion::Scripting {

void InitializePythonBindings();
PyObject* CreateEntityProxy(Scene::Entity* entity);

} // namespace Aetherion::Scripting
