#pragma once

#include <vector>
#include <memory>
#include <stack>
#include "Aetherion/Editor/Command.h"

namespace Aetherion::Editor
{
class CommandHistory
{
public:
    void Push(std::unique_ptr<Command> cmd)
    {
        // Execute the command first
        cmd->Do();

        // If we have redos, clear them
        while (!m_redoStack.empty())
        {
            m_redoStack.pop();
        }

        // Try merge
        if (!m_undoStack.empty())
        {
            if (m_undoStack.top()->Merge(cmd.get()))
            {
                return; // Merged, no need to push
            }
        }

        m_undoStack.push(std::move(cmd));
    }

    void Undo()
    {
        if (m_undoStack.empty())
        {
            return;
        }

        auto cmd = std::move(m_undoStack.top());
        m_undoStack.pop();
        cmd->Undo();
        m_redoStack.push(std::move(cmd));
    }

    void Redo()
    {
        if (m_redoStack.empty())
        {
            return;
        }

        auto cmd = std::move(m_redoStack.top());
        m_redoStack.pop();
        cmd->Do();
        m_undoStack.push(std::move(cmd));
    }

    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }

    void Clear()
    {
        while (!m_undoStack.empty()) m_undoStack.pop();
        while (!m_redoStack.empty()) m_redoStack.pop();
    }

private:
    std::stack<std::unique_ptr<Command>> m_undoStack;
    std::stack<std::unique_ptr<Command>> m_redoStack;
};
} // namespace Aetherion::Editor
