#pragma once

#include <SDL3/SDL.h>

class Application{
private:
    SDL_Window* window;
    SDL_GLContext gl_context;
public:

    Application();
    bool init();
    void run();
    void shutdown();
};

