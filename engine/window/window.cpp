#include "window.h"

GLFWwindow* Window::handle = nullptr;
int Window::width = 0;
int Window::height = 0;
bool Window::isFullscreen = false;
bool Window::isCursorLocked = true;

int Window::windowedX = 100;
int Window::windowedY = 100;
int Window::windowedWidth = 1280;
int Window::windowedHeight = 720;

int Window::init(int w, int h, const char* title) {
    width = w;
    height = h;
    windowedWidth = w;
    windowedHeight = h;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!handle) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(handle);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    glViewport(0, 0, width, height);

    return 0;
}

bool Window::shouldClose() {
    return glfwWindowShouldClose(handle);
}

void Window::update() {
    glfwSwapBuffers(handle);
    glfwPollEvents();
}

void Window::terminate() {
    glfwDestroyWindow(handle);
    glfwTerminate();
}

void Window::toggleFullscreen() {
    if (!isFullscreen) {
        glfwGetWindowPos(handle, &windowedX, &windowedY);
        glfwGetWindowSize(handle, &windowedWidth, &windowedHeight);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        glfwSetWindowMonitor(handle, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        width = mode->width;
        height = mode->height;
        isFullscreen = true;
    } else {
        glfwSetWindowMonitor(handle, nullptr, windowedX, windowedY, windowedWidth, windowedHeight, 0);
        width = windowedWidth;
        height = windowedHeight;
        isFullscreen = false;
    }
    glViewport(0, 0, width, height);
}