#pragma once

#include <glm/glm.hpp>

class Camera {
public:
    Camera(const glm::vec3& position, float yaw, float pitch, float fovY);

    void setPosition(const glm::vec3& p) { m_position = p; }
    void setYaw(float yaw);
    void setPitch(float pitch);

    const glm::vec3& position() const { return m_position; }
    const glm::vec3& forward()  const { return m_forward; }
    const glm::vec3& right()    const { return m_right; }
    const glm::vec3& up()       const { return m_up; }
    float            fovY()     const { return m_fovY; }

private:
    void updateBasis();

    glm::vec3 m_position;
    float     m_yaw;
    float     m_pitch;
    float     m_fovY;

    glm::vec3 m_forward{0.0f, 0.0f, -1.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
};
