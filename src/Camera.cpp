#include "Camera.h"

#include <cmath>

Camera::Camera(const glm::vec3& position, float yaw, float pitch, float fovY)
    : m_position(position), m_yaw(yaw), m_pitch(pitch), m_fovY(fovY) {
    updateBasis();
}

void Camera::setYaw(float yaw) {
    m_yaw = yaw;
    updateBasis();
}

void Camera::setPitch(float pitch) {
    const float limit = glm::radians(89.0f);
    m_pitch = glm::clamp(pitch, -limit, limit);
    updateBasis();
}

void Camera::updateBasis() {
    const float cp = std::cos(m_pitch);
    m_forward = glm::normalize(glm::vec3(
        std::cos(m_yaw) * cp,
        std::sin(m_pitch),
        std::sin(m_yaw) * cp));

    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    m_right = glm::normalize(glm::cross(m_forward, worldUp));
    m_up    = glm::normalize(glm::cross(m_right, m_forward));
}
