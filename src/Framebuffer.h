#pragma once

#include <glad/glad.h>

class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer();

    Framebuffer(const Framebuffer&)            = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void create(int width, int height, GLint internalFormat);
    void resize(int width, int height);
    void bind() const;

    GLuint texture() const { return m_texture; }

private:
    void destroy();

    GLuint m_fbo            = 0;
    GLuint m_texture        = 0;
    GLint  m_internalFormat = GL_RGBA16F;
    int    m_width          = 0;
    int    m_height         = 0;
};
