#pragma once

#include <string>

namespace Aetherion::Editor
{
class Command
{
public:
    virtual ~Command() = default;

    virtual void Do() = 0;
    virtual void Undo() = 0;
    
    // Returns true if the command was merged.
    virtual bool Merge(const Command* other) { return false; }
    
    [[nodiscard]] virtual std::string GetName() const = 0;
    [[nodiscard]] virtual int GetId() const { return -1; }
};
} // namespace Aetherion::Editor
