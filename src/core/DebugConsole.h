#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

/**
 * @file DebugConsole.h
 * @brief In-engine debug console model: commands, history, autocomplete (issue #54).
 *
 * Milestone 9 (In-Engine Editor & Tooling). The headless, renderer-agnostic core
 * behind the ImGui debug console: register commands from engine systems, execute a
 * typed line (tokenized, with quoted arguments) against them, keep a bounded
 * scrollback, navigate input history, and tab-complete command names. Log lines
 * can be mirrored in via print() (see the logging system, #63).
 *
 * The ImGui panel in DebugUI owns the text input and draws the scrollback; keeping
 * the parsing/history/completion here makes it unit-testable without a GL context.
 * Header-only, std only.
 */
namespace IKore {

class DebugConsole {
public:
    /// A command handler: receives the parsed arguments, returns output text.
    using Handler = std::function<std::string(const std::vector<std::string>&)>;

    explicit DebugConsole(std::size_t scrollbackCapacity = 1000)
        : m_capacity(scrollbackCapacity == 0 ? 1 : scrollbackCapacity) {
        registerCommand("help", "list commands", [this](const std::vector<std::string>&) {
            return helpText();
        });
        registerCommand("clear", "clear the console", [this](const std::vector<std::string>&) {
            clear();
            return std::string();
        });
    }

    /// Register (or replace) a command.
    void registerCommand(const std::string& name, const std::string& help, Handler handler) {
        m_commands[name] = Command{help, std::move(handler)};
    }

    bool hasCommand(const std::string& name) const { return m_commands.count(name) > 0; }

    std::vector<std::string> commandNames() const {
        std::vector<std::string> names;
        for (const auto& kv : m_commands) names.push_back(kv.first);
        return names; // std::map iterates in sorted order
    }

    /**
     * @brief Run a command line: echo it, dispatch to the command, append output,
     *        and record it in history. Returns the command's output text.
     */
    std::string execute(const std::string& line) {
        m_historyCursor = -1;
        const std::string trimmed = trim(line);
        if (trimmed.empty()) return std::string();

        print("> " + trimmed);
        m_history.push_back(trimmed);

        const std::vector<std::string> tokens = tokenize(trimmed);
        if (tokens.empty()) return std::string();
        const std::string& name = tokens[0];
        auto it = m_commands.find(name);
        if (it == m_commands.end()) {
            print("unknown command: " + name + " (type 'help')");
            return std::string();
        }
        const std::vector<std::string> args(tokens.begin() + 1, tokens.end());
        const std::string output = it->second.handler(args);
        if (!output.empty()) print(output);
        return output;
    }

    /// Append a raw line to the scrollback (e.g. a mirrored log line).
    void print(const std::string& text) {
        m_lines.push_back(text);
        while (m_lines.size() > m_capacity) m_lines.pop_front();
    }

    void clear() { m_lines.clear(); }
    const std::deque<std::string>& lines() const { return m_lines; }

    /// Recall the previous (older) history entry; the oldest clamps.
    std::string historyPrev() {
        if (m_history.empty()) return std::string();
        if (m_historyCursor == -1) {
            m_historyCursor = static_cast<int>(m_history.size()) - 1;
        } else if (m_historyCursor > 0) {
            --m_historyCursor;
        }
        return m_history[static_cast<std::size_t>(m_historyCursor)];
    }
    /// Recall the next (newer) history entry; past the newest returns empty.
    std::string historyNext() {
        if (m_historyCursor == -1) return std::string();
        if (m_historyCursor < static_cast<int>(m_history.size()) - 1) {
            ++m_historyCursor;
            return m_history[static_cast<std::size_t>(m_historyCursor)];
        }
        m_historyCursor = -1;
        return std::string();
    }

    /// Command names beginning with @p prefix (sorted).
    std::vector<std::string> matches(const std::string& prefix) const {
        std::vector<std::string> out;
        for (const auto& kv : m_commands) {
            if (kv.first.compare(0, prefix.size(), prefix) == 0) out.push_back(kv.first);
        }
        return out;
    }

    /// Longest common completion for @p prefix (the full name if unique).
    std::string complete(const std::string& prefix) const {
        const std::vector<std::string> m = matches(prefix);
        if (m.empty()) return prefix;
        std::string common = m.front();
        for (std::size_t i = 1; i < m.size(); ++i) {
            std::size_t k = 0;
            while (k < common.size() && k < m[i].size() && common[k] == m[i][k]) ++k;
            common.resize(k);
        }
        return common.size() >= prefix.size() ? common : prefix;
    }

    /// Split a line into tokens, honoring double-quoted arguments.
    static std::vector<std::string> tokenize(const std::string& s) {
        std::vector<std::string> out;
        std::string cur;
        bool inQuote = false, hasToken = false;
        for (char c : s) {
            if (inQuote) {
                if (c == '"') {
                    inQuote = false;
                    out.push_back(cur);
                    cur.clear();
                    hasToken = false;
                } else {
                    cur += c;
                }
            } else if (c == '"') {
                inQuote = true;
                hasToken = true;
            } else if (std::isspace(static_cast<unsigned char>(c))) {
                if (hasToken) {
                    out.push_back(cur);
                    cur.clear();
                    hasToken = false;
                }
            } else {
                cur += c;
                hasToken = true;
            }
        }
        if (hasToken) out.push_back(cur);
        return out;
    }

private:
    struct Command {
        std::string help;
        Handler handler;
    };

    static std::string trim(const std::string& s) {
        const std::size_t a = s.find_first_not_of(" \t\r\n");
        const std::size_t b = s.find_last_not_of(" \t\r\n");
        return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
    }

    std::string helpText() const {
        std::string out = "commands:";
        for (const auto& kv : m_commands) out += "\n  " + kv.first + " - " + kv.second.help;
        return out;
    }

    std::map<std::string, Command> m_commands;
    std::deque<std::string> m_lines;
    std::vector<std::string> m_history;
    std::size_t m_capacity;
    int m_historyCursor{-1};
};

} // namespace IKore
