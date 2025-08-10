// Shader.h - Extended to support file-based compilation & caching
#pragma once
#include <string>
#include <memory>
#include <glad/glad.h>

// Forward declares for containers
#include <unordered_map>
#include <mutex>
#include <filesystem>

namespace IKore {

class Shader {
public:
    Shader() = default;
    ~Shader();

    bool loadFromSource(const char* vertexSrc, const char* fragmentSrc, std::string& outError);
    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath, std::string& outError);

    // Cached load: returns shared_ptr that reuses existing compiled program if sources unchanged
    static std::shared_ptr<Shader> loadFromFilesCached(const std::string& vertexPath, const std::string& fragmentPath, std::string& outError);
    static void clearCache();

    void use() const { glUseProgram(m_program); }
    GLuint id() const { return m_program; }

    const std::string& lastLog() const { return m_lastLog; }

    // Uniform setters
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setVec3(const std::string& name, const float* value) const;
    void setMat4(const std::string& name, const float* value) const;
    void setMat3(const std::string& name, const float* value) const;

private:
    GLuint m_program = 0;
    GLuint compile(GLenum type, const char* src, std::string& outError);
    std::string m_lastLog;

    // Caching helpers
    struct CacheEntry {
        std::weak_ptr<Shader> shader;
        std::filesystem::file_time_type vertTime;
        std::filesystem::file_time_type fragTime;
    };
    static std::unordered_map<std::string, CacheEntry> s_cache;
    static std::mutex s_cacheMutex;
    static std::string makeKey(const std::string& v, const std::string& f);
    static bool sourcesUnchanged(const CacheEntry& entry, const std::string& v, const std::string& f);
    static std::string loadFile(const std::string& path, std::string& outError);
    static std::string addLineNumbers(const std::string& src);
};

}
