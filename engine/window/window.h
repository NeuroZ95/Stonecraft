#ifndef WINDOW_H
#define WINDOW_H

#include "../../defs.h"

class Window {
public:
    static GLFWwindow* handle;
    static int width;
    static int height;
    static bool isFullscreen;
    static bool isCursorLocked;

    static int windowedX;
    static int windowedY;
    static int windowedWidth;
    static int windowedHeight;

    static int init(int w, int h, const char* title);
    static bool shouldClose();
    static void update();
    static void terminate();
    static void toggleFullscreen();
};

#endif