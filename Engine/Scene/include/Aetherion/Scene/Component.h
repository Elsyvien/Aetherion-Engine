#pragma once

#include <string>

#include "Aetherion/Core/Types.h"

namespace Aetherion::Scene
{
class Component
{
public:
    Component() = default;
    virtual ~Component() = default;

    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;

    [[nodiscard]] virtual std::string GetDisplayName() const = 0;
    // TODO: Add serialization hooks and editor metadata once components are defined.
};
} // namespace Aetherion::Scene
