#ifndef CAMERA_H
#define CAMERA_H

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>
#include"shaderClass.h"

class Camera {
public:
    glm::vec3 Position;
    glm::vec3 Orientation;
    glm::vec3 Up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 camMatrix = glm::mat4(1.0f);

    int SCR_WIDTH;
    int SCR_HEIGHT;
    
    bool firstClick = true;

    float speed = 0.3f;
    float sensitivity = 0.5f;

    float yaw   = -90.0f; // facing -Z by default
    float pitch = 0.0f;

    Camera(int SCR_WIDTH, int SCR_HEIGHT, glm::vec3 position);

    void updateMatrix(float FOVdeg, float nearPlane, float farPlane);
    void Matrix(Shader& shader, const char* uniform);
    void Inputs(GLFWwindow* window);
};

#endif