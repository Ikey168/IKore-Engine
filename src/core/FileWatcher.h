#pragma once

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>
#include <vector>

/**
 * @file FileWatcher.h
 * @brief Polling file watcher: the mechanism behind hot reload (issue #146).
 *
 * Milestone 10. Register files with a reload callback and call poll() each frame
 * (or every few frames). When a watched file's modification time advances - or a
 * missing file appears - its callback fires. Callbacks are where the engine
 * actually reloads the resource:
 *
 *   watcher.watch("assets/scripts/ai.lua",  [&](auto& p){ scripts.runFile(p); });
 *   watcher.watch("src/shaders/phong.frag", [&](auto& p){ shader.reload(); });
 *   watcher.watch("assets/textures/wall.png",[&](auto& p){ texture.reload(); });
 *
 * Reload failures must not take down the engine: a callback that throws is
 * caught and reported through the error handler, and poll() keeps going. This is
 * a portable, dependency-free polling watcher (std::filesystem) - chosen over
 * inotify/ReadDirectoryChangesW so the same code runs on every platform.
 */
namespace IKore {

class FileWatcher {
public:
    using Callback = std::function<void(const std::string& path)>;
    using ErrorHandler = std::function<void(const std::string& path, const std::string& message)>;

    FileWatcher() : m_onError(defaultErrorHandler()) {}

    /// Replace the error reporter (defaults to printing to stderr).
    void setErrorHandler(ErrorHandler handler) {
        m_onError = handler ? std::move(handler) : defaultErrorHandler();
    }

    /**
     * @brief Watch @p path; @p onChange fires when its mtime advances (or it
     *        appears after being missing). Returns an id for unwatch().
     *
     * The current mtime is recorded now, so an unmodified file does not fire on
     * the first poll().
     */
    std::size_t watch(const std::string& path, Callback onChange) {
        Entry entry;
        entry.id = m_nextId++;
        entry.path = std::filesystem::path(path);
        entry.onChange = std::move(onChange);

        std::error_code ec;
        const auto time = std::filesystem::last_write_time(entry.path, ec);
        if (!ec) {
            entry.exists = true;
            entry.lastWrite = time;
        } else {
            entry.exists = false; // watch a not-yet-created file; fires when it appears
        }
        m_entries.push_back(std::move(entry));
        return m_entries.back().id;
    }

    /// Stop watching the file registered under @p id. Returns true if removed.
    bool unwatch(std::size_t id) {
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (it->id == id) {
                m_entries.erase(it);
                return true;
            }
        }
        return false;
    }

    /// Check every watched file and fire callbacks for those that changed.
    void poll() {
        for (Entry& entry : m_entries) {
            std::error_code ec;
            const bool exists = std::filesystem::exists(entry.path, ec);
            if (ec) {
                report(entry.path, "could not stat file");
                continue;
            }
            if (!exists) {
                entry.exists = false; // mark gone; a later reappearance will fire
                continue;
            }
            const auto time = std::filesystem::last_write_time(entry.path, ec);
            if (ec) {
                report(entry.path, "could not read modification time");
                continue;
            }
            if (!entry.exists || time > entry.lastWrite) {
                entry.exists = true;
                entry.lastWrite = time;
                fire(entry);
            }
        }
    }

    std::size_t count() const { return m_entries.size(); }

private:
    struct Entry {
        std::size_t id{0};
        std::filesystem::path path;
        Callback onChange;
        std::filesystem::file_time_type lastWrite{};
        bool exists{false};
    };

    void fire(const Entry& entry) {
        try {
            if (entry.onChange) entry.onChange(entry.path.string());
        } catch (const std::exception& ex) {
            report(entry.path, std::string("reload failed: ") + ex.what());
        } catch (...) {
            report(entry.path, "reload failed: unknown error");
        }
    }

    void report(const std::filesystem::path& path, const std::string& message) {
        if (m_onError) m_onError(path.string(), message);
    }

    static ErrorHandler defaultErrorHandler() {
        return [](const std::string& path, const std::string& message) {
            std::fprintf(stderr, "[FileWatcher] %s: %s\n", path.c_str(), message.c_str());
        };
    }

    std::vector<Entry> m_entries;
    std::size_t m_nextId{1};
    ErrorHandler m_onError;
};

} // namespace IKore
