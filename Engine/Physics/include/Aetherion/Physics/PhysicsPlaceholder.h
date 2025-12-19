#pragma once

namespace Aetherion::Physics
{
class PhysicsWorldStub
{
public:
    PhysicsWorldStub() = default;
    ~PhysicsWorldStub() = default;

    void Initialize() {}
    void Shutdown() {}
};

// ===============================
// TODO: Physics System Placeholder
// ===============================
// This module will define 2D physics abstractions and integration points.
// No implementation exists at this stage.

inline void TouchPhysicsModule() {}
} // namespace Aetherion::Physics
