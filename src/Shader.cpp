// Shader.cpp - Extended implementation for file-based shader compilation & caching
#include "Shader.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <chrono>

using namespace std::string_literals;

namespace IKore {

std::unordered_map<std::string, Shader::CacheEntry> Shader::s_cache;
std::mutex Shader::s_cacheMutex;

Shader::~Shader(){
    if(m_program){
        glDeleteProgram(m_program);
    }
}

GLuint Shader::compile(GLenum type, const char* src, std::string& outError){
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint success; glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if(!success){
        GLint len; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len);
        glGetShaderInfoLog(shader, len, nullptr, log.data());
    outError.assign(log.begin(), log.end());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Shader::loadFromSource(const char* vertexSrc, const char* fragmentSrc, std::string& outError){
    GLuint vs = compile(GL_VERTEX_SHADER, vertexSrc, outError);
    if(!vs) return false;
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentSrc, outError);
    if(!fs){ glDeleteShader(vs); return false; }

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint success; glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if(!success){
        GLint len; glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len);
        glGetProgramInfoLog(m_program, len, nullptr, log.data());
        outError.assign(log.begin(), log.end());
        glDeleteProgram(m_program);
        m_program = 0;
        return false;
    }
    m_lastLog.clear();
    return true;
}

std::string Shader::loadFile(const std::string& path, std::string& outError){
    std::ifstream file(path);
    if(!file.is_open()){
        outError = "Failed to open shader file: " + path;
        return {};
    }
    std::stringstream ss; ss << file.rdbuf();
    return ss.str();
}

std::string Shader::addLineNumbers(const std::string& src){
    std::stringstream in(src); std::stringstream out;
    std::string line; int lineNo = 1;
    while(std::getline(in, line)){
        out << lineNo++ << ": " << line << '\n';
    }
    return out.str();
}

bool Shader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath, std::string& outError){
    std::string vErr, fErr;
    std::string vSrc = loadFile(vertexPath, vErr);
    if(!vErr.empty()){ outError = vErr; return false; }
    std::string fSrc = loadFile(fragmentPath, fErr);
    if(!fErr.empty()){ outError = fErr; return false; }

    bool ok = loadFromSource(vSrc.c_str(), fSrc.c_str(), outError);
    if(!ok){
        // Provide annotated sources for easier debugging
        m_lastLog = "Vertex Shader (" + vertexPath + ")\n" + addLineNumbers(vSrc) + "\nFragment Shader (" + fragmentPath + ")\n" + addLineNumbers(fSrc) + "\nError: " + outError;
    }
    return ok;
}

std::string Shader::makeKey(const std::string& v, const std::string& f){
    return v + "||" + f;
}

bool Shader::sourcesUnchanged(const CacheEntry& entry, const std::string& v, const std::string& f){
    try {
        auto vt = std::filesystem::last_write_time(v);
        auto ft = std::filesystem::last_write_time(f);
        return vt == entry.vertTime && ft == entry.fragTime;
    } catch(...) { return false; }
}

std::shared_ptr<Shader> Shader::loadFromFilesCached(const std::string& vertexPath, const std::string& fragmentPath, std::string& outError){
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    std::string key = makeKey(vertexPath, fragmentPath);
    auto it = s_cache.find(key);
    if(it != s_cache.end()){
        if(auto existing = it->second.shader.lock()){
            if(sourcesUnchanged(it->second, vertexPath, fragmentPath)){
                return existing; // reuse
            }
        }
    }

    auto shader = std::make_shared<Shader>();
    if(!shader->loadFromFiles(vertexPath, fragmentPath, outError)){
        return nullptr;
    }
    CacheEntry entry;
    entry.shader = shader;
    try {
        entry.vertTime = std::filesystem::last_write_time(vertexPath);
        entry.fragTime = std::filesystem::last_write_time(fragmentPath);
    } catch(...) {}
    s_cache[key] = entry;
    return shader;
}

void Shader::clearCache(){
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_cache.clear();
}

}
