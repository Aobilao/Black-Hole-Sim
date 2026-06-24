#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>
#include <unordered_map>

class Shader {
public:
    Shader(std::string vertPath, std::string fragPath);
    ~Shader();

    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    bool valid() const { return m_program != 0; }
    void use() const { glUseProgram(m_program); }

    bool reload();

    void setInt  (const std::string& name, int v);
    void setFloat(const std::string& name, float v);
    void setVec2 (const std::string& name, const glm::vec2& v);
    void setVec3 (const std::string& name, const glm::vec3& v);

private:
    GLint location(const std::string& name);
    void  destroy();

    GLuint      m_program = 0;
    std::string m_vertPath;
    std::string m_fragPath;
    std::unordered_map<std::string, GLint> m_uniformCache;
};
