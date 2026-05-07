#pragma once

#include <SDL3/SDL.h>
#include "file.hpp"
#define BYTES_PER_COLLUM 0xf


class Application{
private:
    SDL_Window* window;
    SDL_GLContext gl_context;
    FileData file_data;
public:

    Application();
    bool init();
    void run();
    void shutdown();
};

