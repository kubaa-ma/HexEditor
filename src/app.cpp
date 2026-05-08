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
#include <algorithm>
#include <cstring>

#include "app.hpp"

namespace {
    template<typename T>
    T clamp(T val, T lo, T hi) { return val < lo ? lo : (val > hi ? hi : val); }
}

Application::Application()
    : window(nullptr), gl_context(nullptr)
{
}

bool Application::init()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::cerr << "Error in SDL_Init(): " << SDL_GetError() << std::endl;
        return false;
    }

    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                   SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    window = SDL_CreateWindow("HexaEdit",
                              (int)(1280 * main_scale),
                              (int)(800  * main_scale),
                              window_flags);
    if (!window) {
        std::cerr << "Error: SDL_CreateWindow(): " << SDL_GetError() << std::endl;
        return false;
    }

    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::cerr << "Error: SDL_GL_CreateContext(): " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    return true;
}

void Application::shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void Application::render_toolbar()
{
    if (ImGui::Button("Open")) {
        NFD::UniquePathU8 outPath;
        if (NFD::OpenDialog(outPath, nullptr, 0, nullptr) == NFD_OKAY) {
            if (!file_data.load_file(outPath.get())) {
                std::cerr << "Failed to load file: " << outPath.get() << std::endl;
            } else {
                selected_byte    = -1;
                editing_hex      = false;
                editing_ascii    = false;
                search_not_found = false;
            }
        }
    }

    ImGui::SameLine();

    // Disable Save button when no file is loaded
    bool has_file = !file_data.get_filepath().empty();
    if (!has_file) ImGui::BeginDisabled();
    if (ImGui::Button("Save")) {
        if (!file_data.save_file())
            std::cerr << "Failed to save file" << std::endl;
    }
    if (!has_file) ImGui::EndDisabled();

    ImGui::SameLine();

    if (ImGui::Button("Save As...")) {
        NFD::UniquePathU8 outPath;
        if (NFD::SaveDialog(outPath, nullptr, 0, nullptr, nullptr) == NFD_OKAY) {
            if (!file_data.save_file(outPath.get()))
                std::cerr << "Failed to save file" << std::endl;
        }
    }

    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    render_search_bar();
}

void Application::render_search_bar()
{
    ImGui::SetNextItemWidth(220.0f);
    bool enter = ImGui::InputText(
        "##search", search_buf, sizeof(search_buf),
        ImGuiInputTextFlags_EnterReturnsTrue
    );
    ImGui::SameLine();
    ImGui::Checkbox("Hex", &search_as_hex);

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Search for a hex sequence (e.g. 'FF 0A 1B')");

    ImGui::SameLine();
    if (ImGui::Button("Find") || enter)
        do_search();

    if (search_not_found) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Not found");
    }
}

void Application::do_search()
{
    search_not_found = false;
    const auto& buffer = file_data.get_buffer();
    if (buffer.empty() || search_buf[0] == '\0') return;

    std::vector<uint8_t> pattern;
    if (search_as_hex) {
        std::string s(search_buf);
        s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
        for (size_t i = 0; i + 1 < s.size(); i += 2) {
            try { pattern.push_back((uint8_t)std::stoul(s.substr(i, 2), nullptr, 16)); }
            catch (...) { return; }
        }
    } else {
        for (char c : std::string(search_buf))
            pattern.push_back((uint8_t)c);
    }

    if (pattern.empty()) return;

    int buf_size = (int)buffer.size();
    int pat_size = (int)pattern.size();
    int start    = (selected_byte >= 0) ? selected_byte + 1 : 0;

    // Search forward from current position, then wrap around from the beginning
    for (int pass = 0; pass < 2; pass++) {
        int from = (pass == 0) ? start                : 0;
        int to   = (pass == 0) ? buf_size - pat_size  : start;

        for (int i = from; i <= to; i++) {
            if (std::memcmp(buffer.data() + i, pattern.data(), (size_t)pat_size) == 0) {
                selected_byte    = i;
                scroll_to_sel    = true;
                search_not_found = false;
                return;
            }
        }
    }

    search_not_found = true;
}

void Application::render_hex_view()
{
    const auto& buffer = file_data.get_buffer();

    if (buffer.empty()) {
        ImGui::Dummy(ImVec2(0, 20));
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x * 0.5f - 100.0f);
        ImGui::TextDisabled("No file is open.");
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x * 0.5f - 140.0f);
        ImGui::TextDisabled("Use the 'Open' button in the toolbar.");
        return;
    }

    const int total_rows = (int)((buffer.size() + BYTES_PER_COLUMN - 1) / BYTES_PER_COLUMN);

    // Reserve space at the bottom for the status bar
    float footer_h = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    ImVec2 table_size(0.0f, -footer_h);

    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV
                          | ImGuiTableFlags_ScrollY
                          | ImGuiTableFlags_SizingFixedFit
                          | ImGuiTableFlags_RowBg;

    if (!ImGui::BeginTable("##hex_view", 3, flags, table_size))
        return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 75.0f);
    ImGui::TableSetupColumn("Hex",    ImGuiTableColumnFlags_WidthFixed, 400.0f);
    ImGui::TableSetupColumn("ASCII",  ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // Jump to selected byte after search or out-of-view navigation
    if (scroll_to_sel && selected_byte >= 0) {
        float row_h      = ImGui::GetTextLineHeightWithSpacing();
        int   target_row = selected_byte / BYTES_PER_COLUMN;
        float scroll_y   = std::max(0.0f, (target_row - 8) * row_h);
        ImGui::SetScrollY(scroll_y);
        scroll_to_sel = false;
    }

    ImGuiListClipper clipper;
    clipper.Begin(total_rows);

    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
            size_t base = (size_t)row * BYTES_PER_COLUMN;
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("%08X", (unsigned int)base);

            ImGui::TableSetColumnIndex(1);
            for (int j = 0; j < BYTES_PER_COLUMN; j++) {
                size_t idx = base + (size_t)j;

                if (j > 0)
                    ImGui::SameLine(0.0f, j == 8 ? 12.0f : 3.0f); // extra gap at the midpoint

                if (idx >= buffer.size()) {
                    ImGui::TextDisabled("  ");
                    continue;
                }

                bool    is_sel = ((int)idx == selected_byte);
                uint8_t val    = buffer[idx];
                char    label[32];
                snprintf(label, sizeof(label), "%02X##h%zu", val, idx);

                if (is_sel) {
                    ImGui::PushStyleColor(ImGuiCol_Header,        {0.26f, 0.59f, 0.98f, 0.80f});
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0.26f, 0.59f, 0.98f, 0.95f});
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  {0.26f, 0.59f, 0.98f, 1.00f});
                }

                if (ImGui::Selectable(label, is_sel, ImGuiSelectableFlags_None, ImVec2(22.0f, 0.0f))) {
                    selected_byte = (int)idx;
                    editing_hex   = true;
                    editing_ascii = false;
                    edit_nibble   = 0;
                }

                if (is_sel) ImGui::PopStyleColor(3);
            }

            ImGui::TableSetColumnIndex(2);
            for (int j = 0; j < BYTES_PER_COLUMN; j++) {
                size_t idx = base + (size_t)j;

                if (j > 0) ImGui::SameLine(0.0f, 0.0f);

                if (idx >= buffer.size()) {
                    ImGui::TextDisabled(" ");
                    continue;
                }

                bool    is_sel = ((int)idx == selected_byte);
                uint8_t val    = buffer[idx];
                char    ch     = (val >= 32 && val <= 126) ? (char)val : '.';
                char    label[32];
                snprintf(label, sizeof(label), "%c##a%zu", ch, idx);

                if (is_sel) {
                    ImGui::PushStyleColor(ImGuiCol_Header,        {0.26f, 0.59f, 0.98f, 0.80f});
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0.26f, 0.59f, 0.98f, 0.95f});
                }

                if (ImGui::Selectable(label, is_sel, ImGuiSelectableFlags_None, ImVec2(9.0f, 0.0f))) {
                    selected_byte = (int)idx;
                    editing_ascii = true;
                    editing_hex   = false;
                }

                if (is_sel) ImGui::PopStyleColor(2);
            }
        }
    }

    ImGui::EndTable();
}

void Application::render_status_bar()
{
    const auto& buffer = file_data.get_buffer();
    ImGui::Separator();

    if (selected_byte >= 0 && selected_byte < (int)buffer.size()) {
        uint8_t val = buffer[selected_byte];
        ImGui::Text(
            "Offset: 0x%08X (%d)  |  Hex: %02X  |  Dec: %d  |  "
            "Bin: %c%c%c%c %c%c%c%c  |  Char: %c  |  Size: %zu B",
            selected_byte, selected_byte,
            val, (int)val,
            (val & 0x80) ? '1' : '0', (val & 0x40) ? '1' : '0',
            (val & 0x20) ? '1' : '0', (val & 0x10) ? '1' : '0',
            (val & 0x08) ? '1' : '0', (val & 0x04) ? '1' : '0',
            (val & 0x02) ? '1' : '0', (val & 0x01) ? '1' : '0',
            (val >= 32 && val <= 126) ? (char)val : '.',
            buffer.size()
        );
    } else if (!buffer.empty()) {
        ImGui::Text("Size: %zu B  |  Click a byte to edit", buffer.size());
    } else {
        ImGui::TextDisabled("Ready");
    }
}

void Application::handle_keyboard()
{
    if (selected_byte < 0) return;

    auto& buffer  = file_data.get_mutable_buffer();
    int   buf_end = (int)buffer.size() - 1;
    if (buf_end < 0) return;

    ImGuiIO& io = ImGui::GetIO();

    auto nav = [&](int delta) {
        selected_byte = clamp(selected_byte + delta, 0, buf_end);
        scroll_to_sel = true;
        edit_nibble   = 0;
    };

    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) nav(1);
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))  nav(-1);
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))  nav(BYTES_PER_COLUMN);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))    nav(-BYTES_PER_COLUMN);
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown))   nav(BYTES_PER_COLUMN * 16);
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp))     nav(-BYTES_PER_COLUMN * 16);
    if (ImGui::IsKeyPressed(ImGuiKey_Home))       { selected_byte = 0;       scroll_to_sel = true; }
    if (ImGui::IsKeyPressed(ImGuiKey_End))        { selected_byte = buf_end; scroll_to_sel = true; }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        selected_byte = -1;
        editing_hex = editing_ascii = false;
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
        editing_hex   = !editing_hex;
        editing_ascii = !editing_ascii;
        edit_nibble   = 0;
        return;
    }

    if (editing_hex) {
        // Input is processed nibble by nibble; after the low nibble auto-advance to the next byte
        static const struct { ImGuiKey key; uint8_t val; } hex_keys[] = {
            {ImGuiKey_0,0x0},{ImGuiKey_1,0x1},{ImGuiKey_2,0x2},{ImGuiKey_3,0x3},
            {ImGuiKey_4,0x4},{ImGuiKey_5,0x5},{ImGuiKey_6,0x6},{ImGuiKey_7,0x7},
            {ImGuiKey_8,0x8},{ImGuiKey_9,0x9},
            {ImGuiKey_A,0xA},{ImGuiKey_B,0xB},{ImGuiKey_C,0xC},
            {ImGuiKey_D,0xD},{ImGuiKey_E,0xE},{ImGuiKey_F,0xF},
        };
        for (const auto& k : hex_keys) {
            if (ImGui::IsKeyPressed(k.key)) {
                uint8_t& byte = buffer[selected_byte];
                if (edit_nibble == 0) {
                    byte = (byte & 0x0F) | (k.val << 4);
                    edit_nibble = 1;
                } else {
                    byte = (byte & 0xF0) | k.val;
                    edit_nibble = 0;
                    nav(1);
                }
                break;
            }
        }
    }

    if (editing_ascii) {
        for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
            ImWchar c = io.InputQueueCharacters[i];
            if (c >= 32 && c <= 126) {
                buffer[selected_byte] = (uint8_t)c;
                nav(1);
            }
        }
    }
}

void Application::run()
{
    bool done = false;
    ImGuiIO& io = ImGui::GetIO();

    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);

        ImGui::Begin(
            "HexaEdit##main", nullptr,
            ImGuiWindowFlags_NoDecoration  |
            ImGuiWindowFlags_NoMove        |
            ImGuiWindowFlags_NoResize      |
            ImGuiWindowFlags_MenuBar
        );

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open...", "Ctrl+O")) { /* handled by toolbar button */ }
                if (ImGui::MenuItem("Save",    "Ctrl+S")) { file_data.save_file(); }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) done = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                ImGui::Text("Click        - select byte");
                ImGui::Text("Arrow keys   - navigate");
                ImGui::Text("0-9, A-F     - edit hex");
                ImGui::Text("Tab          - toggle hex / ASCII edit");
                ImGui::Text("Esc          - clear selection");
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
            file_data.save_file();

        render_toolbar();
        ImGui::Separator();
        render_hex_view();
        render_status_bar();

        // Only handle keyboard when no text input widget is active
        if (!ImGui::IsAnyItemActive())
            handle_keyboard();

        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }
}