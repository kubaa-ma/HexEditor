#pragma once

#include <SDL3/SDL.h>
#include "imgui.h"
#include "file.hpp"

#define BYTES_PER_COLUMN 0x10 
class Application {
private:
    SDL_Window*   window;
    SDL_GLContext gl_context;
    FileData      file_data;

    int  selected_byte  = -1;
    bool editing_hex    = false;
    bool editing_ascii  = false;
    int  edit_nibble    = 0;
    bool scroll_to_sel  = false;

    char search_buf[256]  = {};
    bool search_as_hex    = false;
    bool search_not_found = false;

    void render_toolbar();
    void render_search_bar();
    void render_hex_view();
    void render_status_bar();
    void handle_keyboard();
    void do_search();

public:
    Application();
    bool init();
    void run();
    void shutdown();
};