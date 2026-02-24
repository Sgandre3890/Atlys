#ifndef WINDOW_H
#define WINDOW_H

#include <GLFW/glfw3.h>  

namespace Window {
    extern GLFWwindow* window;

    void processInput(GLFWwindow *window);
    void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    int initializeWindow(int SCR_WIDTH, int SCR_HEIGHT, const char* title);
    void terminateWindow();
}

#endif
