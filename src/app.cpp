#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include <SDL3/SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif
#include <nfd.hpp>

#include <iostream>
#include <vector>

#include "app.hpp"

Application::Application()
{
    window = nullptr;
    gl_context = nullptr;
}

bool Application::init(){
    if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)){
        std::cout << "Error in SDL_Init(): " << SDL_GetError() << std::endl;
        return false; 
    }
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    window = SDL_CreateWindow("HexaEdit", (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
    if (window == nullptr){
        std::cout << "Error: SDL_CreateWindow(): " <<  SDL_GetError() << std::endl;
        return false;
    }
    gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr){
        std::cout << "Error: SDL_GL_CreateContext(): " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);
    if (!file_data.get_data_from_file()){
        return false;
    }

    return true;
}

void Application::run(){
    bool done = false;
    ImGuiIO& io = ImGui::GetIO();
    ImGuiListClipper clipper;
    
    while (!done){
        
        const auto& buffer = file_data.get_buffer();
        const size_t rows = (buffer.size() + 15) / 16;
        SDL_Event event;

        while (SDL_PollEvent(&event)){
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(window)){
                done = true;
            }
        }

        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED){
            SDL_Delay(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);

        ImGui::Begin(
            "MainWindow",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize
        );

        clipper.Begin(static_cast<int>(rows));
        
       if (ImGui::Button("Open File")){
            NFD::UniquePathU8 outPath;

            nfdresult_t result = NFD::OpenDialog(
                outPath,
                nullptr,
                0,
                nullptr
            );

            if (result == NFD_OKAY){
                if(!file_data.load_file(outPath.get())){
                    std::cout << "Failed to load file" << std::endl;
                }
            }
        }

        while(clipper.Step()){
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++){
                size_t i = row * 16;

                char line[256];

                std::snprintf(
                    line,
                    sizeof(line),
                    "%08X: ",
                    (unsigned int)i
                );

                std::string hex_part = line;
                std::string ascii_part;

                for (int j = 0; j < 16; j++)
                {
                    if (i + j < buffer.size())
                    {
                        uint8_t byte = buffer[i + j];

                        char hex_byte[8];

                        std::snprintf(
                            hex_byte,
                            sizeof(hex_byte),
                            "%02X ",
                            byte
                        );

                        hex_part += hex_byte;

                        if (byte >= 32 && byte <= 126)
                            ascii_part += (char)byte;
                        else
                            ascii_part += '.';
                    }
                    else
                    {
                        hex_part += "   ";
                        ascii_part += ' ';
                    }
                }

                hex_part += " ";
                hex_part += ascii_part;

                ImGui::Text("%s", hex_part.c_str());
            }
        }
        
        ImGui::End();

        ImGui::Render();

        glViewport(
            0,
            0,
            (int)io.DisplaySize.x,
            (int)io.DisplaySize.y
        );

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }
}

void Application::shutdown(){
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}