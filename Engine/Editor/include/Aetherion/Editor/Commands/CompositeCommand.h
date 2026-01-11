#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Aetherion/Editor/Command.h"

namespace Aetherion::Editor
{
class CompositeCommand : public Command
{
public:
    CompositeCommand(std::string name, std::vector<std::unique_ptr<Command>> commands)
        : m_name(std::move(name)), m_commands(std::move(commands))
    {
    }

    void Do() override
    {
        for (auto& cmd : m_commands)
        {
            if (cmd)
            {
                cmd->Do();
            }
        }
    }

    void Undo() override
    {
        for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it)
        {
            if (*it)
            {
                (*it)->Undo();
            }
        }
    }

    [[nodiscard]] std::string GetName() const override
    {
        return m_name.empty() ? "Command Batch" : m_name;
    }

private:
    std::string m_name;
    std::vector<std::unique_ptr<Command>> m_commands;
};
} // namespace Aetherion::Editor
