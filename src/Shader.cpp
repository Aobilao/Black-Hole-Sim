#include "Shader.h"

#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <utility>

namespace {

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::fprintf(stderr, "Failed to open shader file: %s\n", path.c_str());
        return {};
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint compileStage(GLenum type, const std::string& source, const char* label) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len), '\0');
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        std::fprintf(stderr, "[%s shader] compile error:\n%s\n", label, log.c_str());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

}

Shader::Shader(std::string vertPath, std::string fragPath)
    : m_vertPath(std::move(vertPath)), m_fragPath(std::move(fragPath)) {
    reload();
}

Shader::~Shader() { destroy(); }

Shader::Shader(Shader&& other) noexcept
    : m_program(other.m_program),
      m_vertPath(std::move(other.m_vertPath)),
      m_fragPath(std::move(other.m_fragPath)),
      m_uniformCache(std::move(other.m_uniformCache)) {
    other.m_program = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        destroy();
        m_program      = other.m_program;
        m_vertPath     = std::move(other.m_vertPath);
        m_fragPath     = std::move(other.m_fragPath);
        m_uniformCache = std::move(other.m_uniformCache);
        other.m_program = 0;
    }
    return *this;
}

bool Shader::reload() {
    const std::string vertSrc = readFile(m_vertPath);
    const std::string fragSrc = readFile(m_fragPath);
    if (vertSrc.empty() || fragSrc.empty()) return false;

    GLuint vert = compileStage(GL_VERTEX_SHADER, vertSrc, "vertex");
    GLuint frag = compileStage(GL_FRAGMENT_SHADER, fragSrc, "fragment");
    if (!vert || !frag) {
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);
        return false;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len), '\0');
        glGetProgramInfoLog(program, len, nullptr, log.data());
        std::fprintf(stderr, "Shader link error:\n%s\n", log.c_str());
        glDeleteProgram(program);
        return false;
    }

    if (m_program) glDeleteProgram(m_program);
    m_program = program;
    m_uniformCache.clear();  
    return true;
}

GLint Shader::location(const std::string& name) {
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end()) return it->second;
    const GLint loc = glGetUniformLocation(m_program, name.c_str());
    m_uniformCache.emplace(name, loc);
    return loc;
}

void Shader::setInt(const std::string& name, int v) {
    glUniform1i(location(name), v);
}

void Shader::setFloat(const std::string& name, float v) {
    glUniform1f(location(name), v);
}

void Shader::setVec2(const std::string& name, const glm::vec2& v) {
    glUniform2fv(location(name), 1, glm::value_ptr(v));
}

void Shader::setVec3(const std::string& name, const glm::vec3& v) {
    glUniform3fv(location(name), 1, glm::value_ptr(v));
}

void Shader::destroy() {
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}
