#include "Shader.h"
#include <vector>

namespace IKore {

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
    return true;
}

}
