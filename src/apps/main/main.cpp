// Diagram viewer: ImGui + SDL3 + OpenGL3 (C++20)
#define SDL_MAIN_HANDLED

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include <canvas/canvas.hpp>
#include <diagram_loaders/json_loader.hpp>
#include <diagram_loaders/debug_class_diagram.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
    bool auto_overlap_test = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--auto-overlap-test") {
            auto_overlap_test = true;
        }
    }

    SDL_SetMainReady();
    // SDL3: SDL_Init returns true on success, false on failure
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        (void)fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Default fallback if display bounds are unavailable.
    int window_width = 1280;
    int window_height = 720;
    {
        SDL_Rect bounds{};
        if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &bounds)) {
            window_width = bounds.w * 2 / 3;
            window_height = bounds.h * 2 / 3;
        }
    }
    const SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
        | SDL_WINDOW_HIGH_PIXEL_DENSITY;  // HiDPI: request native pixel density back buffer
    SDL_Window* window = SDL_CreateWindow("Diagram", window_width, window_height, window_flags);
    if (!window) {
        (void)fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        (void)fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    // HiDPI: scale fonts and viewports from DPI (backend sets DisplayFramebufferScale)
    io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
    io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;

    ImGui::StyleColorsDark();

    // Load a nice TTF font (replaces the default pixel font for crisp text)ee
    ImGuiIO& io_ref = ImGui::GetIO();
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 2;
    font_cfg.OversampleV = 2;
    font_cfg.PixelSnapH = true;
    const float font_size_px = 19.0f;
#ifdef _WIN32
    const char* font_paths[] = {
        "C:\\Windows\\Fonts\\segoeui.ttf",   // Segoe UI
        "C:\\Windows\\Fonts\\seguiui.ttf",   // Segoe UI (legacy name)
        "C:\\Windows\\Fonts\\arial.ttf",
    };
    bool font_loaded = false;
    for (const char* path : font_paths) {
        if (io_ref.Fonts->AddFontFromFileTTF(path, font_size_px, &font_cfg) != nullptr) {
            font_loaded = true;
            break;
        }
    }
    (void)font_loaded;
#else
    // Linux: try common system font paths
    const char* font_paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
    };
    for (const char* path : font_paths) {
        if (io_ref.Fonts->AddFontFromFileTTF(path, font_size_px, &font_cfg) != nullptr)
            break;
    }
#endif

    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    std::optional<diagram_model::ClassDiagram> class_diagram;
    const char* class_paths[] = { "data/example_class_diagram.json", "example_class_diagram.json" };
    for (const char* path : class_paths) {
        auto loaded = diagram_loaders::load_class_diagram_from_json_file(path);
        if (loaded) {
            class_diagram = std::move(*loaded);
            break;
        }
    }
    if (!class_diagram)
        class_diagram = diagram_loaders::generate_debug_class_diagram();

    canvas::DiagramCanvas diagram_canvas;
    if (class_diagram)
        diagram_canvas.set_class_diagram(&*class_diagram);

    bool running = true;
    int frame = 0;
    struct ToggleAction {
        int frame_index;
        const char* id;
        bool expanded;
    };
    const std::vector<ToggleAction> auto_actions = {
        {10, "NPC", true},
        {35, "UIElement", true},
        {60, "Bow", true},
        {85, "Sword", true},
        {110, "WallTile", true},
        {150, "NPC", false},
        {175, "UIElement", false},
        {200, "Bow", false},
        {225, "Sword", false},
        {250, "WallTile", false},
        {290, "NPC", true},
        {315, "UIElement", true},
    };
    std::size_t next_action = 0;
    int settled_frames = 0;
    const int max_test_frames = 1200;
    int test_exit_code = 0;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(window))
                running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Diagram", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        if (canvas_size.x > 0 && canvas_size.y > 0) {
            ImGui::BeginChild("canvas", canvas_size, false, ImGuiWindowFlags_NoScrollbar);
            diagram_canvas.update_and_draw(canvas_size.x, canvas_size.y);
            ImGui::EndChild();
        }
        ImGui::End();

        if (auto_overlap_test) {
            while (next_action < auto_actions.size() && frame >= auto_actions[next_action].frame_index) {
                diagram_canvas.set_class_block_expanded(auto_actions[next_action].id, auto_actions[next_action].expanded);
                ++next_action;
            }

            const bool all_actions_done = next_action >= auto_actions.size();
            const bool settled = diagram_canvas.is_layout_settled();
            if (all_actions_done && settled) {
                ++settled_frames;
            } else {
                settled_frames = 0;
            }

            if (settled_frames >= 30 || frame >= max_test_frames) {
                const std::size_t overlaps = diagram_canvas.current_overlap_count();
                (void)fprintf(stderr,
                    "[auto-overlap-test] finished frame=%d settled=%d overlap_count=%zu\n",
                    frame, settled ? 1 : 0, overlaps);
                test_exit_code = overlaps == 0 ? 0 : 2;
                running = false;
            }
        }

        ImGui::Render();
        SDL_GL_MakeCurrent(window, gl_context);
        // HiDPI: use framebuffer size in pixels, not logical DisplaySize
        const int fb_w = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
        const int fb_h = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
        ++frame;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (auto_overlap_test) {
        return test_exit_code;
    }
    return 0;
}
