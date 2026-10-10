// Capture scripted input through the real editor, without changing the shipped app.
#define SDL_MAIN_HANDLED
#define main gatehaven_application_main
#include "../src/app/main.cpp"
#undef main

#include <cstdio>
#include <fstream>
#include <sstream>

int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("Usage: capture_demo TIMELINE OUTPUT_DIRECTORY");
        const std::filesystem::path output(argv[2]);
        std::filesystem::create_directories(output);
        const auto document = output / "First circuit.ghv";
        SDL_SetMainReady();
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
        if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
        const SdlLifetime lifetime;
        SDL_Window* raw_window{}; SDL_Renderer* raw_renderer{};
        if (!SDL_CreateWindowAndRenderer("Gatehaven demo", 1280, 800, SDL_WINDOW_HIDDEN,
                                        &raw_window, &raw_renderer)) throw std::runtime_error(SDL_GetError());
        const std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(raw_window, SDL_DestroyWindow);
        const std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(raw_renderer, SDL_DestroyRenderer);
        SDL_SetRenderLogicalPresentation(renderer.get(), 1280, 800, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        const ui::FontAtlas fonts(renderer.get());
        const ui::SymbolAtlas symbols(renderer.get());
        TestDirectory temporary;
        temporary.path = std::filesystem::temp_directory_path() /
            ("gatehaven-demo-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        auto clipboard = ClipboardSession::join(temporary.path);
        if (!clipboard) throw std::runtime_error(clipboard.error());
        std::optional<std::filesystem::path> reopen;
        App app(window.get(), **clipboard,
            [&](std::optional<std::filesystem::path> path) -> std::expected<void, std::string> { reopen = path; return {}; },
            ui::launch_demo, confirm_overwrite, show_inspection,
            [&](std::unique_ptr<DialogRequest> request, SDL_Window*) {
                const auto filename = path_utf8(document);
                const char* paths[]{filename.c_str(), nullptr};
                dialog_callback(request.release(), paths, 0);
            });
        app.start_blank(); app.view.center_x = app.view.center_y = 0.5;
        std::map<int, std::vector<std::string>> schedule;
        std::ifstream timeline(argv[1]);
        if (!timeline) throw std::runtime_error("Cannot read timeline");
        std::string row;
        while (std::getline(timeline, row)) {
            std::istringstream input(row); int frame{};
            if (input >> frame) { std::getline(input, row); schedule[frame].push_back(row); }
        }
        float pointer_x = 440, pointer_y = 160;
        bool pressed = false;
        int pulse_until = -1;
        bool saw_on = false, saw_off = false, saved = false, reopened = false;
        Circuit saved_circuit;
        for (int frame = 0; frame < 1500; ++frame) {
            for (const auto& command : schedule[frame]) {
                std::istringstream input(command); std::string action; input >> action;
                SDL_Event event{};
                if (action == "move") {
                    float x{}, y{}; input >> x >> y;
                    event.type = SDL_EVENT_MOUSE_MOTION;
                    event.motion.x = x; event.motion.y = y;
                    event.motion.xrel = x - pointer_x; event.motion.yrel = y - pointer_y;
                    pointer_x = x; pointer_y = y; app.event(event);
                } else if (action == "down" || action == "up") {
                    int button = SDL_BUTTON_LEFT; input >> button;
                    event.type = action == "down" ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
                    event.button.button = static_cast<Uint8>(button); event.button.clicks = 1;
                    event.button.x = pointer_x; event.button.y = pointer_y; app.event(event);
                    pressed = action == "down";
                    if (pressed) pulse_until = frame + 12;
                } else if (action == "key") {
                    std::string name; int control = 0; input >> name >> control;
                    event.type = SDL_EVENT_KEY_DOWN;
                    event.key.key = SDL_GetKeyFromName(name.c_str());
                    event.key.mod = control ? SDL_KMOD_CTRL : SDL_KMOD_NONE;
                    if (!event.key.key) throw std::runtime_error("Unknown key: " + name);
                    app.event(event);
                } else if (action == "wheel") {
                    float amount{}; input >> amount;
                    event.type = SDL_EVENT_MOUSE_WHEEL; event.wheel.y = amount;
                    event.wheel.mouse_x = pointer_x; event.wheel.mouse_y = pointer_y; app.event(event);
                } else if (action == "on" || action == "off") {
                    const bool on = app.simulation.powered({4, 0});
                    if (on != (action == "on")) throw std::runtime_error("AND output did not match the demo caption");
                    if (on) saw_on = true; else saw_off = true;
                } else if (action == "saved") {
                    auto loaded = load_document(document);
                    if (!loaded || *loaded != app.simulation.document_snapshot(app.circuit))
                        throw std::runtime_error("Demo save did not preserve the circuit");
                    saved_circuit = *loaded; saved = true;
                } else if (action == "example") {
                    std::string name; input >> name;
                    if (!app.start_example(name)) throw std::runtime_error("Unknown demo lesson");
                } else if (action == "screenshot") {
                    std::string filename; input >> filename;
                    app.render(renderer.get());
                    const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
                        SDL_RenderReadPixels(renderer.get(), nullptr), SDL_DestroySurface);
                    if (!surface || !SDL_SaveBMP(surface.get(), (output / filename).string().c_str()))
                        throw std::runtime_error(SDL_GetError());
                } else throw std::runtime_error("Unknown timeline action: " + action);
            }
            app.update(1.0 / 30.0);
            if (reopen) {
                if (!app.open(*reopen) || app.circuit != saved_circuit)
                    throw std::runtime_error("Demo reopen changed the saved circuit");
                reopen.reset(); reopened = true;
            }
            app.render(renderer.get());
            // A capture-only pointer makes scripted clicks visible in the film.
            if (frame < pulse_until) {
                SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_BLEND);
                const float radius = 12 + static_cast<float>(12 - (pulse_until - frame));
                std::array<SDL_FPoint, 49> ring{};
                for (std::size_t i = 0; i < ring.size(); ++i) {
                    const double angle = static_cast<double>(i) / 48 * 6.283185307;
                    ring[i] = {pointer_x + radius * static_cast<float>(std::cos(angle)),
                               pointer_y + radius * static_cast<float>(std::sin(angle))};
                }
                SDL_SetRenderDrawColor(renderer.get(), 0, 124, 108, 150);
                SDL_RenderLines(renderer.get(), ring.data(), static_cast<int>(ring.size()));
            }
            const SDL_Color ink{28, 44, 55, 255};
            const SDL_Color fill = pressed ? SDL_Color{0, 141, 119, 255} : SDL_Color{255, 255, 255, 255};
            const SDL_Vertex cursor[]{
                {{pointer_x, pointer_y}, {fill.r/255.F, fill.g/255.F, fill.b/255.F, 1}, {}},
                {{pointer_x + 3, pointer_y + 19}, {fill.r/255.F, fill.g/255.F, fill.b/255.F, 1}, {}},
                {{pointer_x + 8, pointer_y + 12}, {fill.r/255.F, fill.g/255.F, fill.b/255.F, 1}, {}},
                {{pointer_x + 16, pointer_y + 10}, {fill.r/255.F, fill.g/255.F, fill.b/255.F, 1}, {}}};
            const int indices[]{0,1,2,0,2,3};
            SDL_RenderGeometry(renderer.get(), nullptr, cursor, 4, indices, 6);
            const SDL_FPoint outline[]{{pointer_x,pointer_y},{pointer_x+3,pointer_y+19},
                {pointer_x+8,pointer_y+12},{pointer_x+16,pointer_y+10},{pointer_x,pointer_y}};
            SDL_SetRenderDrawColor(renderer.get(), ink.r, ink.g, ink.b, ink.a);
            SDL_RenderLines(renderer.get(), outline, 5);
            const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> pixels(
                SDL_RenderReadPixels(renderer.get(), nullptr), SDL_DestroySurface);
            const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> rgb(
                pixels ? SDL_ConvertSurface(pixels.get(), SDL_PIXELFORMAT_RGB24) : nullptr, SDL_DestroySurface);
            if (!rgb || rgb->w != 1280 || rgb->h != 800) throw std::runtime_error("Unexpected capture dimensions");
            for (int y = 0; y < rgb->h; ++y) {
                const auto* bytes = static_cast<const unsigned char*>(rgb->pixels) + y * rgb->pitch;
                if (std::fwrite(bytes, 1, 1280 * 3, stdout) != 1280 * 3) throw std::runtime_error("Frame pipe closed");
            }
        }
        if (!saw_on || !saw_off || !saved || !reopened) throw std::runtime_error("Incomplete demo workflow");
        std::ofstream evidence(output / "capture-evidence.json");
        evidence << "{\"source_revision\":\"" << source_revision << "\",\"frames\":1500,\"fps\":30,"
                    "\"renderer\":\"software\",\"scripted_input\":true,\"and_output_on_off_verified\":true,"
                    "\"save_reopen_verified\":true,\"manual_acceptance\":false}\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "Demo capture: " << error.what() << '\n'; return 1; }
}
