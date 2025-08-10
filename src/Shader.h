#pragma once
#include <string>
#include <glad/glad.h>

namespace IKore {

class Shader {
public:
    Shader() = default;
    ~Shader();

    bool loadFromSource(const char* vertexSrc, const char* fragmentSrc, std::string& outError);
    void use() const { glUseProgram(m_program); }
    GLuint id() const { return m_program; }

private:
    GLuint m_program = 0;
    GLuint compile(GLenum type, const char* src, std::string& outError);
};

}
