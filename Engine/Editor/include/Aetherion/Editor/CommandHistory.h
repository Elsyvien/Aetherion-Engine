#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <stack>
#include <vector>
#include "Aetherion/Editor/Command.h"

namespace Aetherion::Editor
{
enum class CommandAuditAction
{
    Do,
    Undo,
    Redo,
    Merge
};

struct CommandAuditEntry
{
    std::uint64_t sequence{0};
    std::uint64_t timestampMs{0};
    CommandAuditAction action{CommandAuditAction::Do};
    std::string commandName;
    std::string source;
    std::string summary;
    std::string requestId;
};

class CommandHistory
{
public:
    void Push(std::unique_ptr<Command> cmd)
    {
        // Execute the command first
        cmd->Do();
        RecordAudit(*cmd, CommandAuditAction::Do);

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
                RecordAudit(*cmd, CommandAuditAction::Merge);
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
        RecordAudit(*cmd, CommandAuditAction::Undo);
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
        RecordAudit(*cmd, CommandAuditAction::Redo);
        m_undoStack.push(std::move(cmd));
    }

    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }

    void Clear()
    {
        while (!m_undoStack.empty()) m_undoStack.pop();
        while (!m_redoStack.empty()) m_redoStack.pop();
    }

    [[nodiscard]] const std::vector<CommandAuditEntry>& GetAuditLog() const noexcept
    {
        return m_auditLog;
    }

    void ClearAuditLog() noexcept
    {
        m_auditLog.clear();
        m_auditSequence = 0;
    }

private:
    void RecordAudit(const Command& cmd, CommandAuditAction action)
    {
        using namespace std::chrono;
        CommandAuditEntry entry;
        entry.sequence = ++m_auditSequence;
        entry.timestampMs = static_cast<std::uint64_t>(
            duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
        entry.action = action;
        entry.commandName = cmd.GetName();
        const CommandContext& context = cmd.GetContext();
        entry.source = context.source;
        entry.summary = context.summary;
        entry.requestId = context.requestId;
        m_auditLog.push_back(std::move(entry));
    }

    std::stack<std::unique_ptr<Command>> m_undoStack;
    std::stack<std::unique_ptr<Command>> m_redoStack;
    std::vector<CommandAuditEntry> m_auditLog;
    std::uint64_t m_auditSequence{0};
};
} // namespace Aetherion::Editor
