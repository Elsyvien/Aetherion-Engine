#pragma once

#include <string>

namespace Aetherion::Editor
{
struct CommandContext
{
    std::string source{"Editor"};
    std::string summary;
    std::string requestId;
};

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

    void SetContext(const CommandContext& context) { m_context = context; }
    [[nodiscard]] const CommandContext& GetContext() const { return m_context; }

private:
    CommandContext m_context{};
};
} // namespace Aetherion::Editor
