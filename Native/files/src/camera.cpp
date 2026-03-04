#include "camera.h"
#include <algorithm>

Camera::Camera(glm::vec3 position)
    : Position(position), WorldUp(0.0f, 1.0f, 0.0f) {
    updateVectors();
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(Position, Position + Front, Up);
}

void Camera::ProcessKeyboard(CameraMovement dir, float dt) {
    float v = Speed * dt;
    if (dir == CameraMovement::FORWARD)  Position += Front * v;
    if (dir == CameraMovement::BACKWARD) Position -= Front * v;
    if (dir == CameraMovement::LEFT)     Position -= Right * v;
    if (dir == CameraMovement::RIGHT)    Position += Right * v;
    if (dir == CameraMovement::UP)       Position += WorldUp * v;
    if (dir == CameraMovement::DOWN)     Position -= WorldUp * v;
}

void Camera::ProcessMouseMovement(float xoff, float yoff, bool constrainPitch) {
    xoff *= Sensitivity;
    yoff *= Sensitivity;
    Yaw   += xoff;
    Pitch += yoff;
    if (constrainPitch)
        Pitch = std::clamp(Pitch, -89.0f, 89.0f);
    updateVectors();
}

void Camera::ProcessMouseScroll(float yoff) {
    Zoom = std::clamp(Zoom - yoff, 1.0f, 90.0f);
}

void Camera::updateVectors() {
    glm::vec3 f;
    f.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    f.y = sin(glm::radians(Pitch));
    f.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(f);
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up    = glm::normalize(glm::cross(Right, Front));
}
