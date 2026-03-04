#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum class CameraMovement { FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

class Camera {
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Yaw   = -90.0f;
    float Pitch =   0.0f;
    float Speed      = 5.0f;
    float Sensitivity = 0.1f;
    float Zoom = 45.0f;

    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f));

    glm::mat4 GetViewMatrix() const;
    void ProcessKeyboard(CameraMovement dir, float dt);
    void ProcessMouseMovement(float xoff, float yoff, bool constrainPitch = true);
    void ProcessMouseScroll(float yoff);

private:
    void updateVectors();
};
