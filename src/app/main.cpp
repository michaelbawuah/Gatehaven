#include "font.hpp"
#include "instances.hpp"
#include "gatehaven/clipboard_session.hpp"
#include "gatehaven/document.hpp"
#include "gatehaven/inspection.hpp"
#include "gatehaven/document_path.hpp"
#include "gatehaven/editor.hpp"
#include "gatehaven/stamp_preview.hpp"
#include "gatehaven/examples.hpp"
#include "gatehaven/simulation.hpp"
#include "gatehaven/selection.hpp"
#include "gatehaven/polyline.hpp"
#include "gatehaven/preferences.hpp"
#include "gatehaven/file_io.hpp"
#include "gatehaven/file_endpoints.hpp"
#include "gatehaven/viewport.hpp"
#include "gatehaven/touch.hpp"
#include "gatehaven/taps.hpp"
#include "gatehaven/recovery.hpp"
#include "gatehaven/file_time.hpp"
#include "gatehaven/recovery_schedule.hpp"
#include "gatehaven/version.hpp"
#include "gatehaven/paths.hpp"
#include "gatehaven/resources.hpp"
#include "gatehaven/process.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

namespace {
using namespace gatehaven;
constexpr SDL_Color ink{31, 45, 64, 255};
constexpr SDL_Color muted{87, 105, 122, 255};
constexpr SDL_Color teal{0, 133, 111, 255};
constexpr SDL_Color orange{220, 98, 40, 255};
constexpr SDL_Color paper{246, 248, 248, 255};
constexpr SDL_Color white{255, 255, 255, 255};
constexpr SDL_Color border{217, 225, 229, 255};
constexpr std::array palette{Element::wire, Element::crossing, Element::source,
    Element::signal, Element::and_gate, Element::or_gate, Element::nand_gate,
    Element::nor_gate, Element::positive_relay, Element::negative_relay,
    Element::screen, Element::file_input, Element::file_output};
constexpr std::array<std::string_view, 13> labels{"WIRE", "CROSSING", "SOURCE", "SIGNAL",
    "AND", "OR", "NAND", "NOR", "+ RELAY", "- RELAY", "SCREEN", "FILE IN", "FILE OUT"};

constexpr std::array<SDL_Color, 6> binding_colors{{{205, 63, 64, 255}, {53, 103, 205, 255},
    {36, 139, 74, 255}, {0, 150, 180, 255}, {179, 62, 169, 255}, {190, 153, 0, 255}}};
constexpr std::array<std::string_view, 6> binding_names{"LEFT", "RIGHT", "MIDDLE", "X1", "X2", "TOUCH"};

std::optional<std::size_t> input_button(const SDL_MouseButtonEvent& event) {
    if (event.which == SDL_TOUCH_MOUSEID) return 5;
    switch (event.button) {
    case SDL_BUTTON_LEFT: return 0;
    case SDL_BUTTON_RIGHT: return 1;
    case SDL_BUTTON_MIDDLE: return 2;
    case SDL_BUTTON_X1: return 3;
    case SDL_BUTTON_X2: return 4;
    default: return std::nullopt;
    }
}

void rectangle(SDL_Renderer* r, float x, float y, float w, float h, SDL_Color c, bool outline = false) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    const SDL_FRect rect{x, y, w, h};
    if (outline) SDL_RenderRect(r, &rect); else SDL_RenderFillRect(r, &rect);
}

void line(SDL_Renderer* r, float x, float y, float xx, float yy, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderLine(r, x, y, xx, yy);
}

struct Button { ViewRect rect; std::string label; };
std::array<Button, 10> buttons(bool running, unsigned speed) {
    return {{{{272, 22, 102, 40}, running ? "PAUSE" : "PLAY"},
             {{384, 22, 68, 40}, "STEP"}, {{462, 22, 80, 40}, "RESET"},
             {{552, 22, 68, 40}, "UNDO"}, {{630, 22, 68, 40}, "REDO"},
             {{708, 22, 68, 40}, "OPEN"}, {{786, 22, 68, 40}, "SAVE"},
             {{864, 22, 80, 40}, "FRAME"}, {{954, 22, 130, 40}, std::to_string(speed) + " TICKS/S"},
             {{1094, 22, 156, 40}, "NEW CIRCUIT"}}};
}

struct DialogResult { bool save{}; bool canceled{}; std::string path; std::string error; std::optional<Point> endpoint; int filter{}; };
struct Mailbox { std::mutex mutex; std::optional<DialogResult> result; };
struct DialogRequest { std::shared_ptr<Mailbox> mailbox; bool save; std::string location; std::optional<Point> endpoint; };

void SDLCALL dialog_callback(void* userdata, const char* const* paths, int filter) {
    // Ownership crosses the C API once; the callback reclaims it even on cancel.
    std::unique_ptr<DialogRequest> request(static_cast<DialogRequest*>(userdata));
    DialogResult result;
    result.save = request->save;
    result.filter = filter;
    result.endpoint = request->endpoint;
    if (!paths) result.error = SDL_GetError();
    else if (!paths[0]) result.canceled = true;
    else result.path = paths[0];
    const std::lock_guard lock(request->mailbox->mutex);
    request->mailbox->result = std::move(result);
}

using OverwritePrompt = std::function<bool(SDL_Window*)>;
bool confirm_overwrite(SDL_Window* window) {
    const SDL_MessageBoxButtonData choices[]{
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "Cancel"},
        {0, 1, "Replace changed file"}};
    const SDL_MessageBoxData data{SDL_MESSAGEBOX_WARNING, window, "File changed outside Gatehaven",
        "This file changed since you opened or saved it. Replace it with this circuit? Cancel and use Save As to keep both versions.", 2, choices, nullptr};
    int choice = 0;
    return SDL_ShowMessageBox(&data, &choice) && choice == 1;
}

using InspectionDialog = std::function<bool(SDL_Window*, const std::string&)>;
bool show_inspection(SDL_Window* window, const std::string& text) {
    return SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Gatehaven cell inspector", text.c_str(), window);
}

class App {
public:
    Circuit circuit = starter_circuit();
    Simulation simulation;
    History history;
    Viewport view;
    bool running{};
    bool quit{};

    explicit App(SDL_Window* window, ClipboardSession& clipboards,
                 ui::InstanceLauncher launcher = ui::launch_instance, ui::DemoLauncher demos = ui::launch_demo, OverwritePrompt overwrite = confirm_overwrite, InspectionDialog inspection = show_inspection)
        : window_(window), clipboards_(clipboards), launcher_(std::move(launcher)), demo_launcher_(std::move(demos)), confirm_overwrite_(std::move(overwrite)), inspection_dialog_(std::move(inspection)) {
        simulation.initialize(circuit);
        view.area = {240, 96, 1040, 668};
        view.frame(circuit.bounds());
    }

    void start_blank() {
        circuit.clear();
        simulation.initialize(circuit);
        view.frame(std::nullopt);
        status_ = "NEW CIRCUIT - CHOOSE A COMPONENT TO BEGIN";
    }

    void launch_open(const std::filesystem::path& path) { launch(path); }

    bool start_example(std::string_view name) {
        auto example = make_example(name);
        if (!example) return false;
        circuit = std::move(*example);
        simulation.initialize(circuit);
        view.frame(circuit.bounds());
        status_ = "EXAMPLE: " + std::string(name) + " - I: INTERACT, SPACE: PLAY";
        return true;
    }

    bool prepare_snapshot(std::string_view state) {
        if (state == "help") help_ = true;
        else if (state == "examples") examples_menu_ = true;
        else if (state == "keyboard") keyboard_focus_ = 2;
        else if (state == "canvas") { keyboard_cursor_ = Point{0, 0}; hover_ = keyboard_cursor_; view.center_on(*keyboard_cursor_); status_ = "KEYBOARD CANVAS - ARROWS: NAVIGATE, F8: INSPECT, F10: STEP, ESC: EXIT"; }
        else if (state == "recovery") {
            recovery_menu_ = true;
            const auto date = std::chrono::sys_days{std::chrono::year{2026}/10/9};
            recovery_entries_.push_back({"preview", file_time(date), 2048});
        }
        else if (state == "hints") { beginner_ = true; tools_[0] = {ToolKind::interactor}; }
        else if (state == "speed") edit_speed();
        else if (state == "clipboard") clipboard_menu_ = 'v';
        else return start_example(state);
        return true;
    }

    void load_settings(const std::filesystem::path& path) {
        preferences_path_ = path;
        std::error_code error;
        if (!std::filesystem::exists(path, error)) return;
        const auto bytes = read_bounded_file(path, 4096);
        const auto settings = bytes ? decode_preferences(*bytes) : std::expected<Preferences, std::string>(std::unexpected(bytes.error()));
        if (!settings) { status_ = "SETTINGS IGNORED: " + settings.error(); return; }
        tools_ = settings->bindings; speed_ = settings->speed; beginner_ = settings->beginner;
    }

    void enable_recovery(const std::filesystem::path& directory) {
        auto opened = RecoveryStore::open(directory);
        if (!opened) { status_ = "RECOVERY UNAVAILABLE: " + opened.error(); return; }
        recovery_ = std::move(*opened);
    }

    void show_recovery(bool only_if_available = false) {
        if (!recovery_) return;
        cancel_gesture();
        const auto entries = recovery_->scan();
        if (!entries) { status_ = "RECOVERY: " + entries.error(); return; }
        recovery_entries_ = *entries; recovery_index_ = 0; recovery_delete_ = false;
        recovery_menu_ = !only_if_available || !recovery_entries_.empty(); help_ = false; examples_menu_ = false;
    }

    void finish_recovery() {
        if (recovery_) {
            const auto cleared = recovery_->discard();
            if (!cleared) std::cerr << "Gatehaven recovery cleanup: " << cleared.error() << '\n';
        }
    }

    void save_settings() const {
        if (preferences_path_.empty()) return;
        const auto saved = replace_file(preferences_path_, encode_preferences({tools_, speed_, beginner_}));
        if (!saved) std::cerr << "Gatehaven preferences: " << saved.error() << '\n';
    }

    bool open(const std::filesystem::path& path) {
        const auto before = fingerprint_file(path);
        if (!before || !*before) { status_ = before ? "DOCUMENT NO LONGER EXISTS" : before.error(); return false; }
        auto loaded = load_document(path);
        if (!loaded) {
            status_ = "LINE " + std::to_string(loaded.error().line) + ": " + loaded.error().message;
            return false;
        }
        const auto after = fingerprint_file(path);
        if (!after || *after != *before) { status_ = "DOCUMENT CHANGED WHILE OPENING - TRY AGAIN"; return false; }
        circuit = std::move(*loaded);
        path_ = path; disk_version_ = *after;
        keyboard_cursor_.reset();
        history.clear();
        simulation.initialize(circuit);
        endpoints_.clear();
        selection_.clear();
        placing_ = false;
        running = false;
        history.mark_saved();
        view.frame(circuit.bounds());
        status_ = "DOCUMENT OPENED";
        return true;
    }

    void update(double elapsed) {
        if (discard_elapsed_) { elapsed = 0; discard_elapsed_ = false; }
        if (!std::isfinite(elapsed) || elapsed < 0) elapsed = 0;
        if (recovery_) {
            if (recovery_schedule_.poll(circuit.revision(), history.modified(), elapsed)) checkpoint();
            recovery_cleanup_wait_ = std::max(0.0, recovery_cleanup_wait_ - std::clamp(elapsed, 0.0, 30.0));
            if (!history.modified() && recovery_dirty_ && recovery_cleanup_wait_ == 0) clear_checkpoint();
        }
        const auto file_name = path_.filename().u8string();
        const std::string title = (history.modified() ? "* " : "") +
            (path_.empty() ? std::string("Untitled") : std::string(file_name.begin(), file_name.end())) + " - Gatehaven";
        if (title != title_) { SDL_SetWindowTitle(window_, title.c_str()); title_ = title; }
        std::optional<DialogResult> result;
        {
            const std::lock_guard lock(mailbox_->mutex);
            result = std::move(mailbox_->result);
            mailbox_->result.reset();
        }
        if (result) {
            dialog_pending_ = false;
            if (!result->error.empty()) status_ = "DIALOG: " + result->error;
            else if (result->canceled) status_ = "CANCELED";
            else {
                auto path = utf8_path(result->path);
                if (result->endpoint) {
                    const auto expected_type = result->save ? Element::file_output : Element::file_input;
                    if (circuit.at(*result->endpoint) != expected_type) status_ = "FILE PORT WAS REMOVED";
                    else {
                        const auto chosen = result->save ? endpoints_.choose_output(*result->endpoint, path) : endpoints_.choose_input(*result->endpoint, path);
                        status_ = chosen ? "COMMUNICATOR FILE CONNECTED" : chosen.error();
                    }
                } else if (result->save) {
                    path = document_save_path(std::move(path), result->filter);
                    if (save(path) && close_after_save_) quit = true;
                } else launch_open(path);
            }
            close_after_save_ = false;
        }
        if (!running || recovery_menu_ || help_ || dialog_pending_ || clipboard_menu_ || speed_edit_ || examples_menu_) { accumulator_ = 0; return; }
        accumulator_ += std::clamp(elapsed, 0.0, 0.25);
        const double interval = 1.0 / speed_;
        unsigned work = 0;
        while (accumulator_ >= interval && work < 8) {
            tick();
            accumulator_ -= interval;
            ++work;
        }
        if (work == 8) accumulator_ = std::fmod(accumulator_, interval);
    }

    void event(const SDL_Event& e) {
        if (e.type == SDL_EVENT_QUIT) {
            if (!dialog_pending_ && discard_changes()) quit = true;
            return;
        }
        if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST || e.type == SDL_EVENT_WINDOW_MINIMIZED || e.type == SDL_EVENT_WINDOW_HIDDEN) {
            cancel_gesture(); keyboard_focus_.reset(); accumulator_ = 0; checkpoint();
        }
        if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE && !keyboard_cursor_) hover_.reset();
        if (e.type == SDL_EVENT_KEY_UP && e.key.key == SDLK_E) eyedropper_ = false;
        if (e.type == SDL_EVENT_KEY_UP && (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) && keyboard_cursor_) {
            interaction_button_.reset(); endpoints_.release_screens();
        }
        if (dialog_pending_) return;
        if (e.type == SDL_EVENT_FINGER_DOWN || e.type == SDL_EVENT_FINGER_MOTION ||
            e.type == SDL_EVENT_FINGER_UP || e.type == SDL_EVENT_FINGER_CANCELED) { touch_event(e); return; }
        if (touches_.size() != 0 &&
            ((e.type == SDL_EVENT_MOUSE_MOTION && e.motion.which != SDL_TOUCH_MOUSEID) ||
             ((e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) && e.button.which != SDL_TOUCH_MOUSEID))) return;
        if (e.type == SDL_EVENT_DROP_FILE && e.drop.data) {
            launch_open(utf8_path(e.drop.data)); return;
        }
        if (e.type == SDL_EVENT_KEY_DOWN && (!e.key.repeat || (keyboard_cursor_ &&
            (e.key.key == SDLK_LEFT || e.key.key == SDLK_RIGHT || e.key.key == SDLK_UP || e.key.key == SDLK_DOWN)))) key(e.key);
        if (help_) {
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) help_ = false;
            return;
        }
        if (recovery_menu_) {
            if (recovery_delete_) return;
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                const auto start = (recovery_index_ / 5) * 5;
                for (std::size_t i = start; i < std::min(start + 5, recovery_entries_.size()); ++i) {
                    if (recovery_button(i - start).contains(e.button.x, e.button.y)) { restore_recovery(i); break; }
                }
            }
            return;
        }
        if (examples_menu_) {
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                for (std::size_t i = 0; i < example_names.size(); ++i) {
                    if (example_button(i).contains(e.button.x, e.button.y)) { choose_example(i); break; }
                }
            }
            return;
        }
        if (speed_edit_) {
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                if (ViewRect{656, 460, 148, 40}.contains(e.button.x, e.button.y)) commit_speed();
                else if (ViewRect{484, 460, 148, 40}.contains(e.button.x, e.button.y)) speed_edit_.reset();
            }
            return;
        }
        if (clipboard_menu_) {
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                for (unsigned slot = 0; slot < 10; ++slot) {
                    if (clipboard_button(slot).contains(e.button.x, e.button.y)) choose_clipboard(slot);
                    if (!clipboard_menu_) break;
                }
            }
            return;
        }
        if (e.type == SDL_EVENT_MOUSE_WHEEL && view.area.contains(e.wheel.mouse_x, e.wheel.mouse_y)) {
            const double amount = e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -e.wheel.y : e.wheel.y;
            view.zoom(std::pow(1.18, amount), e.wheel.mouse_x, e.wheel.mouse_y);
            if (drag_ && hover_) update_preview(*hover_);
            if (polyline_ && hover_) polyline_preview(*hover_);
        }
        if (e.type == SDL_EVENT_MOUSE_MOTION) {
            if (pan_button_) { view.pan(e.motion.xrel, e.motion.yrel); pan_distance_ += std::abs(e.motion.xrel) + std::abs(e.motion.yrel); }
            if (!keyboard_cursor_) hover_ = view.area.contains(e.motion.x, e.motion.y) ? view.cell(e.motion.x, e.motion.y) : std::nullopt;
            if (drag_ && hover_) update_preview(*hover_);
            if (polyline_ && hover_) polyline_preview(*hover_);
            if (interaction_button_) {
                endpoints_.release_screens();
                if (hover_ && circuit.at(*hover_) == Element::screen) endpoints_.hold_screen(*hover_);
            }
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) { keyboard_focus_.reset(); keyboard_cursor_.reset(); mouse_down(e.button); }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && input_button(e.button) == interaction_button_) {
            interaction_button_.reset(); endpoints_.release_screens();
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && pan_button_ && input_button(e.button) == pan_button_) {
            if (pan_distance_ < 3 && pan_origin_ && pan_clicks_ >= 2) view.center_on(*pan_origin_);
            pan_button_.reset(); pan_origin_.reset();
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && drag_ && input_button(e.button) == drag_button_) {
            const auto end = view.area.contains(e.button.x, e.button.y) ? view.cell(e.button.x, e.button.y) : std::nullopt;
            if (end) {
                update_preview(*end, true);
                if (drag_tool_.kind == ToolKind::selector) {
                    const Bounds region{{std::min(drag_->x, end->x), std::min(drag_->y, end->y)},
                                        {std::max(drag_->x, end->x), std::max(drag_->y, end->y)}};
                    selection_.combine(Selection::rectangle(circuit, region), selection_mode_);
                    selection_changed_ = false;
                    status_ = "SELECTION READY - CTRL C TO COPY";
                } else apply(preview_);
            }
            drag_.reset();
            preview_.clear();
        }
    }

    void render(SDL_Renderer* r) const {
        SDL_SetRenderDrawColor(r, paper.r, paper.g, paper.b, 255);
        SDL_RenderClear(r);
        const SDL_Rect clip{240, 96, 1040, 668};
        SDL_SetRenderClipRect(r, &clip);
        const auto visible = view.visible();
        const auto first = view.screen(visible.min);
        if (view.scale >= 10) {
            for (double x = first.first; x < 1280; x += view.scale) {
                line(r, static_cast<float>(x), 96, static_cast<float>(x), 764, {230, 236, 238, 255});
            }
            for (double y = first.second; y < 764; y += view.scale) {
                line(r, 240, static_cast<float>(y), 1280, static_cast<float>(y), {230, 236, 238, 255});
            }
        }
        circuit.visit(visible, [&](const Cell& cell) {
            draw_cell(r, cell, simulation.ports(cell.position));
            if (selection_.contains(cell.position)) {
                const auto [x, y] = view.screen(cell.position);
                rectangle(r, static_cast<float>(x + 2), static_cast<float>(y + 2),
                    static_cast<float>(view.scale - 4), static_cast<float>(view.scale - 4),
                    selection_changed_ ? SDL_Color{205, 63, 64, 255} : SDL_Color{53, 103, 205, 255}, true);
            }
        });
        for (const auto& cell : preview_) {
            if (visible.contains(cell.position)) draw_cell(r, cell, 0, true);
        }
        if (placing_ && hover_) {
            placement_preview_.visit(*hover_, visible, [&](Cell cell) { draw_cell(r, cell, 0, true); });
        }
        if (selection_) draw_selection(r, *selection_.bounds());
        if (drag_tool_.kind == ToolKind::selector && drag_ && hover_) {
            draw_selection(r, {{std::min(drag_->x, hover_->x), std::min(drag_->y, hover_->y)},
                               {std::max(drag_->x, hover_->x), std::max(drag_->y, hover_->y)}});
        }
        if (hover_) {
            const auto [x, y] = view.screen(*hover_);
            rectangle(r, static_cast<float>(x), static_cast<float>(y), static_cast<float>(view.scale),
                      static_cast<float>(view.scale), orange, true);
        }
        SDL_SetRenderClipRect(r, nullptr);
        rectangle(r, 0, 0, 1280, 96, white);
        rectangle(r, 0, 96, 240, 668, white);
        line(r, 0, 95, 1280, 95, border);
        line(r, 239, 96, 239, 764, border);
        ui::text(r, 22, 24, "GATEHAVEN", ink, 3);
        ui::text(r, 24, 57, "BUILD. CONNECT. DISCOVER.", muted, 1.25F);
        const auto toolbar = buttons(running, speed_);
        for (std::size_t i = 0; i < toolbar.size(); ++i) {
            const auto& button = toolbar[i];
            rectangle(r, static_cast<float>(button.rect.x), static_cast<float>(button.rect.y),
                      static_cast<float>(button.rect.width), static_cast<float>(button.rect.height),
                      i == 0 ? teal : paper);
            const auto text_width = static_cast<float>(button.label.size()) * 9;
            ui::text(r, static_cast<float>(button.rect.x + button.rect.width / 2) - text_width / 2,
                     37, button.label, i == 0 ? white : ink, 1.5F);
        }
        ui::text(r, 276, 76, "SPACE: PLAY / PAUSE    ARROWS: MOVE SELECTION    HOLD E: EYEDROPPER    SCROLL: ZOOM", muted, 1.25F);
        ui::text(r, 22, 121, "COMPONENTS", muted, 1.5F);
        for (std::size_t i = 0; i < palette.size(); ++i) {
            const float y = 146 + static_cast<float>(i) * 28;
            const InputTool tool{ToolKind::pencil, palette[i]};
            const bool selected = tools_[0] == tool;
            rectangle(r, 12, y, 216, 26, selected ? SDL_Color{225, 241, 236, 255} : white);
            if (selected) rectangle(r, 12, y, 3, 26, teal);
            if (i < 10) ui::text(r, 26, y + 8, std::to_string((i + 1) % 10), muted, 1.5F);
            ui::text(r, 52, y + 7, labels[i], selected ? teal : ink, 1.75F);
            draw_bindings(r, y + 6, tool);
        }
        constexpr std::array kinds{ToolKind::selector, ToolKind::panner, ToolKind::eraser, ToolKind::interactor};
        constexpr std::array<std::string_view, 4> tool_labels{"Q  SELECT", "PAN", "ERASE", "I  INTERACT"};
        for (std::size_t i = 0; i < kinds.size(); ++i) {
            const float y = 522 + static_cast<float>(i) * 28;
            const InputTool tool{kinds[i]};
            const bool selected = tools_[0] == tool;
            rectangle(r, 12, y, 216, 26, selected ? SDL_Color{225, 241, 236, 255} : paper);
            ui::text(r, 26, y + 8, tool_labels[i], selected ? teal : ink, 1.5F);
            draw_bindings(r, y + 6, tool);
        }
        ui::text(r, 22, 645, "CLICK A TOOL TO BIND", ink, 1.25F);
        for (std::size_t i = 0; i < binding_names.size(); ++i) {
            ui::text(r, 22 + static_cast<float>(i % 3) * 68, 667 + static_cast<float>(i / 3) * 15,
                     binding_names[i], binding_colors[i], 1);
        }
        ui::text(r, 22, 707, "SHARED CLIPBOARD " + std::to_string(clipboard_), ink, 1.25F);
        ui::text(r, 22, 729, "CTRL SHIFT C/V: CHOOSE", muted, 1.0F);
        ui::text(r, 22, 748, "B: HINTS  F3: EXAMPLES", muted, 1.0F);
        rectangle(r, 0, 764, 1280, 36, ink);
        ui::text(r, 20, 771, status_.substr(0, 73), white, 1.25F);
        if (hover_) {
            const auto info = std::string(name(circuit.at(*hover_))) + "  [" + std::to_string(hover_->x) +
                ", " + std::to_string(hover_->y) + "]  " + (is_communicator(circuit.at(*hover_))
                    ? std::string("TX ") + (simulation.sent(*hover_) ? "ON" : "OFF") + "  RX " + (simulation.received(*hover_) ? "ON" : "OFF")
                    : circuit.at(*hover_) == Element::crossing
                        ? std::string("H ") + ((simulation.ports(*hover_) & 10U) ? "ON" : "OFF") + "  V " + ((simulation.ports(*hover_) & 5U) ? "ON" : "OFF")
                        : (circuit.at(*hover_) == Element::positive_relay || circuit.at(*hover_) == Element::negative_relay)
                            ? std::string("POWER ") + (simulation.powered(*hover_) ? "ON" : "OFF") + "  CONDUCT " + (simulation.conductive(*hover_) ? "ON" : "OFF")
                            : (simulation.powered(*hover_) ? "ON" : "OFF"));
            ui::text(r, 20, 786, info, {170, 208, 196, 255}, 1);
        }
        ui::text(r, 786, 777, (history.modified() ? "*  " : "") + std::to_string(circuit.size()) + " CELLS    TICK " +
                 std::to_string(simulation.ticks()) + (running ? "    RUNNING" : "    PAUSED"), white, 1.25F);
        if (beginner_ && !recovery_menu_ && !help_ && !examples_menu_ && !clipboard_menu_ && !speed_edit_ && !dialog_pending_) render_hint(r);
        if (keyboard_focus_) {
            const auto box = focus_rect(*keyboard_focus_);
            rectangle(r, static_cast<float>(box.x - 2), static_cast<float>(box.y - 2),
                static_cast<float>(box.width + 4), static_cast<float>(box.height + 4), orange, true);
        }
        if (help_) render_help(r);
        if (examples_menu_) render_examples(r);
        if (recovery_menu_) render_recovery(r);
        if (clipboard_menu_) render_clipboard_menu(r);
        if (speed_edit_) render_speed_dialog(r);
        if (dialog_pending_) {
            rectangle(r, 414, 334, 548, 92, ink);
            ui::text(r, 440, 373, "CHOOSE A FILE IN THE SYSTEM DIALOG", white, 2);
        }
    }

private:
    SDL_Window* window_; // Non-owning; main owns the window for the entire App lifetime.
    std::array<InputTool, 6> tools_{Preferences{}.bindings};
    InputTool drag_tool_;
    bool eyedropper_{};
    std::optional<std::size_t> keyboard_focus_;
    std::optional<Point> keyboard_cursor_;
    TouchGesture touches_;
    TapSequence taps_;
    Uint8 touch_clicks_{1};
    bool touch_canvas_{};
    bool placing_{};
    bool help_{};
    bool examples_menu_{};
    bool recovery_menu_{};
    bool recovery_delete_{};
    std::vector<RecoveryEntry> recovery_entries_;
    std::size_t recovery_index_{};
    std::size_t example_index_{};
    bool beginner_{};
    bool dialog_pending_{};
    bool close_after_save_{};
    std::optional<Point> hover_;
    std::optional<Point> drag_;
    std::size_t drag_button_{};
    std::optional<std::size_t> pan_button_;
    std::optional<Point> pan_origin_;
    float pan_distance_{};
    Uint8 pan_clicks_{};
    std::optional<std::size_t> interaction_button_;
    FileEndpoints endpoints_;
    std::vector<Cell> preview_;
    Selection selection_;
    SelectionMode selection_mode_{SelectionMode::replace};
    bool selection_changed_{};
    std::optional<Polyline> polyline_;
    std::size_t polyline_button_{};
    ClipboardSession& clipboards_;
    ui::InstanceLauncher launcher_;
    ui::DemoLauncher demo_launcher_;
    OverwritePrompt confirm_overwrite_;
    InspectionDialog inspection_dialog_;
    Stamp placement_;
    StampPreview placement_preview_;
    unsigned clipboard_{};
    std::optional<char> clipboard_menu_;
    std::filesystem::path path_;
    std::optional<FileFingerprint> disk_version_;
    std::filesystem::path preferences_path_;
    std::string title_;
    std::string status_{"WELCOME - EXPLORE THE STARTER CIRCUIT"};
    std::shared_ptr<Mailbox> mailbox_{std::make_shared<Mailbox>()};
    unsigned speed_{5};
    std::optional<std::string> speed_edit_;
    bool speed_replace_{};
    bool speed_error_{};
    double accumulator_{};
    bool discard_elapsed_{};
    std::unique_ptr<RecoveryStore> recovery_;
    RecoverySchedule recovery_schedule_;
    bool recovery_dirty_{};
    double recovery_cleanup_wait_{};

    void checkpoint() {
        if (!recovery_ || !history.modified()) return;
        const auto saved = recovery_->write(simulation.document_snapshot(circuit));
        if (saved) { recovery_dirty_ = true; recovery_schedule_.written(circuit.revision()); }
        else { recovery_schedule_.failed(); status_ = "RECOVERY SAVE FAILED: " + saved.error(); }
    }
    void clear_checkpoint() {
        if (!recovery_) return;
        const auto cleared = recovery_->discard();
        if (cleared) { recovery_dirty_ = false; recovery_cleanup_wait_ = 0; }
        else { recovery_cleanup_wait_ = 30; status_ = "RECOVERY CLEANUP FAILED: " + cleared.error(); }
    }

    void cancel_gesture(bool clear_touch = true) {
        if (clear_touch) touches_.clear();
        taps_.clear();
        drag_.reset();
        polyline_.reset();
        pan_button_.reset();
        pan_origin_.reset(); pan_distance_ = 0;
        interaction_button_.reset(); endpoints_.release_screens();
        preview_.clear();
        eyedropper_ = false;
    }

    void touch_event(const SDL_Event& event) {
        if (event.type == SDL_EVENT_FINGER_CANCELED) { cancel_gesture(); return; }
        const TouchId id{event.tfinger.touchID, event.tfinger.fingerID};
        const TouchPoint point{event.tfinger.x, event.tfinger.y};
        if (event.type == SDL_EVENT_FINGER_DOWN) {
            if (touches_.size() == 0) touch_canvas_ = view.area.contains(point.x, point.y);
            else touch_canvas_ = touch_canvas_ && view.area.contains(point.x, point.y);
        }
        const auto update = event.type == SDL_EVENT_FINGER_DOWN ? touches_.down(id, point) :
            event.type == SDL_EVENT_FINGER_UP ? touches_.up(id, point) : touches_.move(id, point);
        if (update.action == TouchAction::cancel) { cancel_gesture(false); return; }
        if (update.action == TouchAction::navigate) {
            if (touch_canvas_) {
                view.zoom(update.zoom, update.before.x, update.before.y);
                view.pan(update.after.x - update.before.x, update.after.y - update.before.y);
            }
            return;
        }
        if (update.action == TouchAction::none) return;
        const auto time = event.tfinger.timestamp != 0 ? event.tfinger.timestamp : SDL_GetTicksNS();
        if (update.action == TouchAction::begin) touch_clicks_ = static_cast<Uint8>(taps_.begin(event.tfinger.touchID, time, update.after));
        else if (update.action == TouchAction::move) taps_.move(update.after);
        else if (update.action == TouchAction::end) taps_.end(time, update.after);
        SDL_Event pointer{};
        if (update.action == TouchAction::move) {
            pointer.type = SDL_EVENT_MOUSE_MOTION; pointer.motion.which = SDL_TOUCH_MOUSEID;
            pointer.motion.x = static_cast<float>(update.after.x); pointer.motion.y = static_cast<float>(update.after.y);
            pointer.motion.xrel = static_cast<float>(update.after.x - update.before.x);
            pointer.motion.yrel = static_cast<float>(update.after.y - update.before.y);
        } else {
            pointer.type = update.action == TouchAction::begin ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
            pointer.button.which = SDL_TOUCH_MOUSEID; pointer.button.button = SDL_BUTTON_LEFT; pointer.button.clicks = touch_clicks_;
            pointer.button.x = static_cast<float>(update.after.x); pointer.button.y = static_cast<float>(update.after.y);
        }
        this->event(pointer);
    }

    bool apply(std::span<const Cell> edits) {
        const auto result = history.apply(circuit, edits);
        if (!result) { status_ = result.error(); return false; }
        if (*result) {
            
            simulation.invalidate(history.last_changes());
            simulation.refresh(circuit);
            endpoints_.prune(circuit);
            status_ = "CIRCUIT UPDATED";
        }
        return true;
    }

    void update_preview(Point end, bool complete = false) {
        preview_.clear();
        if (!drag_ || drag_tool_.kind == ToolKind::selector) return;
        auto element = drag_tool_.kind == ToolKind::eraser ? Element::empty : drag_tool_.element;
        if (*drag_ == end && circuit.at(end) == element && element >= Element::positive_relay) {
            element = Element::signal;
        }
        const auto stroke = complete ? pencil_line(*drag_, end, element) : clipped_pencil_line(*drag_, end, element, view.visible());
        if (stroke) preview_ = *stroke; else status_ = stroke.error();
    }

    void mouse_down(const SDL_MouseButtonEvent& e) {
        const auto button = input_button(e);
        if (!button) return;
        if (help_) { help_ = false; return; }
        if (e.button == SDL_BUTTON_LEFT) {
            const auto toolbar = buttons(running, speed_);
            for (std::size_t i = 0; i < toolbar.size(); ++i) {
                if (!toolbar[i].rect.contains(e.x, e.y)) continue;
                switch (i) {
                case 0: running = !running; accumulator_ = 0; break;
                case 1: running = false; tick(); break;
                case 2: reset_simulation(); break;
                case 3: undo(false); break;
                case 4: undo(true); break;
                case 5: file_dialog(false); break;
                case 6: request_save((SDL_GetModState() & SDL_KMOD_SHIFT) != 0); break;
                case 7: view.frame(circuit.bounds()); break;
                case 8: edit_speed(); break;
                case 9: fresh(); break;
                default: break;
                }
                return;
            }
        }
        if (ViewRect{12, 146, 216, 364}.contains(e.x, e.y)) {
            cancel_gesture();
            tools_[*button] = {ToolKind::pencil, palette[static_cast<std::size_t>((e.y - 146) / 28)]};
            placing_ = false; return;
        }
        if (ViewRect{12, 522, 216, 112}.contains(e.x, e.y)) {
            cancel_gesture();
            constexpr std::array kinds{ToolKind::selector, ToolKind::panner, ToolKind::eraser, ToolKind::interactor};
            tools_[*button] = {kinds[static_cast<std::size_t>((e.y - 522) / 28)]};
            placing_ = false; return;
        }
        if (!view.area.contains(e.x, e.y)) return;
        hover_ = view.cell(e.x, e.y);
        if (!hover_) return;
        if (polyline_) {
            if (*button != polyline_button_) return;
            const auto added = polyline_->append(*hover_);
            if (!added) { status_ = added.error(); return; }
            if ((SDL_GetModState() & SDL_KMOD_SHIFT) == 0 || e.clicks >= 2) {
                const auto edits = polyline_->edits();
                if (edits && apply(*edits)) { polyline_.reset(); preview_.clear(); }
            } else polyline_preview(*hover_);
            return;
        }
        if (eyedropper_) {
            const auto element = circuit.at(*hover_);
            tools_[*button] = element == Element::empty ? InputTool{ToolKind::eraser} : InputTool{ToolKind::pencil, element};
            status_ = "BOUND " + std::string(binding_names[*button]) + " TO " + std::string(name(element));
            placing_ = false; return;
        }
        if (placing_ && e.button == SDL_BUTTON_LEFT) {
            const auto edits = paste(placement_, *hover_);
            if (!edits) { status_ = edits.error(); return; }
            if (apply(*edits)) {
                std::set<Point> points;
                for (const auto& cell : *edits) points.insert(cell.position);
                selection_ = Selection::points(std::move(points));
                selection_.set_frame({*hover_, *translated(*hover_, placement_.width - 1, placement_.height - 1)});
                selection_changed_ = true; placing_ = false;
            }
            return;
        }
        if (drag_ || pan_button_) return; // One gesture at a time; release its owning button to finish.
        if (tools_[*button].kind == ToolKind::panner) { pan_button_ = *button; pan_origin_ = hover_; pan_distance_ = 0; pan_clicks_ = e.clicks; return; }
        if (tools_[*button].kind == ToolKind::interactor) {
            if (circuit.at(*hover_) == Element::screen) { interaction_button_ = *button; endpoints_.hold_screen(*hover_); }
            else if (is_communicator(circuit.at(*hover_))) communicator_dialog(*hover_);
            else status_ = "CHOOSE A COMMUNICATOR TO INTERACT";
            return;
        }
        const auto modifiers = SDL_GetModState();
        selection_mode_ = (modifiers & SDL_KMOD_ALT) != 0 ? SelectionMode::subtract :
            (modifiers & SDL_KMOD_SHIFT) != 0 ? SelectionMode::add : SelectionMode::replace;
        if (tools_[*button].kind == ToolKind::selector && e.clicks >= 2) {
            selection_.combine(connected_selection(circuit, *hover_, e.clicks >= 3), selection_mode_);
            selection_changed_ = false; return;
        }
        if ((modifiers & SDL_KMOD_SHIFT) != 0 &&
            (tools_[*button].kind == ToolKind::pencil || tools_[*button].kind == ToolKind::eraser)) {
            polyline_.emplace(*hover_, tools_[*button].kind == ToolKind::eraser ? Element::empty : tools_[*button].element);
            polyline_button_ = *button;
            polyline_preview(*hover_);
            status_ = "POLYLINE: CLICK TO ADD. BACKSPACE TO RETRACE. DOUBLE CLICK TO FINISH.";
            return;
        }
        drag_ = hover_;
        drag_button_ = *button;
        drag_tool_ = tools_[*button];
        update_preview(*drag_);
    }

    static constexpr std::size_t focus_count = 10 + palette.size() + 4;
    ViewRect focus_rect(std::size_t index) const {
        if (index < 10) return buttons(running, speed_)[index].rect;
        if (index < 10 + palette.size()) return {12, 146 + static_cast<double>(index - 10) * 28, 216, 26};
        return {12, 522 + static_cast<double>(index - 10 - palette.size()) * 28, 216, 26};
    }

    void key(SDL_KeyboardEvent e) {
        if (e.key >= SDLK_KP_1 && e.key <= SDLK_KP_9) e.key = SDLK_1 + (e.key - SDLK_KP_1);
        else if (e.key == SDLK_KP_0) e.key = SDLK_0;
        else if (e.key == SDLK_KP_ENTER) e.key = SDLK_RETURN;
        const bool control = (e.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
        const bool shift = (e.mod & SDL_KMOD_SHIFT) != 0;
        if (recovery_menu_) {
            if (recovery_delete_) {
                if (e.key == SDLK_ESCAPE) recovery_delete_ = false;
                else if (e.key == SDLK_RETURN || e.key == SDLK_KP_ENTER) delete_recovery();
                return;
            }
            if (e.key == SDLK_DELETE && !recovery_entries_.empty()) { recovery_delete_ = true; return; }
            if (e.key == SDLK_ESCAPE || e.key == SDLK_F4) recovery_menu_ = false;
            else if (!recovery_entries_.empty()) {
                if (e.key == SDLK_UP) recovery_index_ = (recovery_index_ + recovery_entries_.size() - 1) % recovery_entries_.size();
                if (e.key == SDLK_DOWN) recovery_index_ = (recovery_index_ + 1) % recovery_entries_.size();
                if (e.key == SDLK_HOME) recovery_index_ = 0;
                if (e.key == SDLK_END) recovery_index_ = recovery_entries_.size() - 1;
                if (e.key == SDLK_PAGEUP) recovery_index_ = recovery_index_ > 5 ? recovery_index_ - 5 : 0;
                if (e.key == SDLK_PAGEDOWN) recovery_index_ = std::min(recovery_index_ + 5, recovery_entries_.size() - 1);
                if (e.key == SDLK_RETURN || e.key == SDLK_KP_ENTER) restore_recovery(recovery_index_);
            }
            return;
        }
        if (speed_edit_) { speed_key(e.key); return; }
        if (examples_menu_) {
            if (e.key == SDLK_ESCAPE || e.key == SDLK_F3) examples_menu_ = false;
            else if (e.key >= SDLK_1 && e.key <= SDLK_6) choose_example(static_cast<std::size_t>(e.key - SDLK_1));
            else if (e.key == SDLK_UP) example_index_ = (example_index_ + example_names.size() - 1) % example_names.size();
            else if (e.key == SDLK_DOWN) example_index_ = (example_index_ + 1) % example_names.size();
            else if (e.key == SDLK_RETURN) choose_example(example_index_);
            return;
        }
        if (help_) {
            if (e.key == SDLK_ESCAPE || e.key == SDLK_F2) help_ = false;
            return;
        }
        if (clipboard_menu_) {
            if (e.key == SDLK_ESCAPE) clipboard_menu_.reset();
            else if (e.key >= SDLK_0 && e.key <= SDLK_9) choose_clipboard(static_cast<unsigned>(e.key - SDLK_0));
            else if (e.key == SDLK_LEFT) clipboard_ = (clipboard_ + 9) % 10;
            else if (e.key == SDLK_RIGHT) clipboard_ = (clipboard_ + 1) % 10;
            else if (e.key == SDLK_RETURN) choose_clipboard(clipboard_);
            return;
        }
        if (e.key == SDLK_TAB && !control) {
            cancel_gesture();
            keyboard_focus_ = keyboard_focus_ ? (*keyboard_focus_ + (shift ? focus_count - 1 : 1)) % focus_count : (shift ? focus_count - 1 : 0);
            status_ = "KEYBOARD CONTROLS - TAB: NEXT, SHIFT TAB: PREVIOUS, ENTER: ACTIVATE, ESC: CANVAS";
            return;
        }
        if (keyboard_focus_ && !control) {
            if (e.key == SDLK_ESCAPE) { keyboard_focus_.reset(); return; }
            if (e.key == SDLK_LEFT || e.key == SDLK_UP) { keyboard_focus_ = (*keyboard_focus_ + focus_count - 1) % focus_count; return; }
            if (e.key == SDLK_RIGHT || e.key == SDLK_DOWN) { keyboard_focus_ = (*keyboard_focus_ + 1) % focus_count; return; }
            if (e.key == SDLK_RETURN || e.key == SDLK_KP_ENTER || e.key == SDLK_SPACE) {
                const auto box = focus_rect(*keyboard_focus_);
                SDL_MouseButtonEvent click{}; click.button = SDL_BUTTON_LEFT; click.clicks = 1;
                if (shift && *keyboard_focus_ >= 10) click.which = SDL_TOUCH_MOUSEID;
                click.x = static_cast<float>(box.x + box.width / 2); click.y = static_cast<float>(box.y + box.height / 2);
                const bool palette_control = *keyboard_focus_ >= 10;
                mouse_down(click); if (palette_control) keyboard_focus_.reset(); return;
            }
        }
        if (polyline_ && e.key == SDLK_BACKSPACE) {
            if (!polyline_->backtrack()) { polyline_.reset(); preview_.clear(); }
            else polyline_preview(hover_.value_or(polyline_->vertices().back()));
            return;
        }
        if (polyline_ && e.key == SDLK_RETURN) {
            const auto edits = polyline_->edits();
            if (edits && apply(*edits)) { polyline_.reset(); preview_.clear(); }
            return;
        }
        if (e.key == SDLK_ESCAPE) {
            cancel_gesture(); keyboard_cursor_.reset(); selection_.clear(); placing_ = false; help_ = false; return;
        }
        if (keyboard_cursor_ && !control && (e.key == SDLK_RETURN || e.key == SDLK_KP_ENTER)) {
            view.center_on(*keyboard_cursor_);
            const auto [x, y] = view.screen(*keyboard_cursor_);
            SDL_MouseButtonEvent click{}; click.button = SDL_BUTTON_LEFT; click.clicks = 1;
            click.x = static_cast<float>(x + view.scale / 2); click.y = static_cast<float>(y + view.scale / 2);
            mouse_down(click);
            if (interaction_button_) return; // A screen stays held until Enter is released.
            SDL_Event release{}; release.button = click; release.type = SDL_EVENT_MOUSE_BUTTON_UP;
            event(release);
            return;
        }
        if (keyboard_cursor_ && (e.key == SDLK_LEFT || e.key == SDLK_RIGHT || e.key == SDLK_UP || e.key == SDLK_DOWN)) {
            const std::int64_t distance = control ? 4 : 1;
            const auto dx = e.key == SDLK_LEFT ? -distance : e.key == SDLK_RIGHT ? distance : 0;
            const auto dy = e.key == SDLK_UP ? -distance : e.key == SDLK_DOWN ? distance : 0;
            const auto next = translated(*keyboard_cursor_, dx, dy);
            if (next) { keyboard_cursor_ = next; hover_ = next; view.center_on(*next); }
            else status_ = "CANVAS COORDINATE LIMIT REACHED";
            return;
        }
        if (selection_ && !placing_ && (e.key == SDLK_LEFT || e.key == SDLK_RIGHT || e.key == SDLK_UP || e.key == SDLK_DOWN)) {
            const std::int64_t distance = control ? 4 : 1;
            const auto dx = e.key == SDLK_LEFT ? -distance : e.key == SDLK_RIGHT ? distance : 0;
            const auto dy = e.key == SDLK_UP ? -distance : e.key == SDLK_DOWN ? distance : 0;
            const auto moved = move_selection(simulation.document_snapshot(circuit), selection_, dx, dy);
            if (!moved) status_ = moved.error();
            else if (apply(moved->edits)) { selection_ = moved->selection; selection_changed_ = true; }
            return;
        }
        if (control) {
            switch (e.key) {
            case SDLK_S: request_save(shift); break;
            case SDLK_O: file_dialog(false); break;
            case SDLK_N: fresh(); break;
            case SDLK_Z: undo(shift); break;
            case SDLK_Y: undo(true); break;
            case SDLK_A:
                selection_ = circuit.bounds() ? Selection::rectangle(circuit, *circuit.bounds()) : Selection{};
                selection_changed_ = false; tools_[0] = {ToolKind::selector}; break;
            case SDLK_C: clipboard_action('c', shift); break;
            case SDLK_X: clipboard_action('x', shift); break;
            case SDLK_V: clipboard_action('v', shift); break;
            case SDLK_D:
                cancel_gesture(); placement_ = capture_selection(simulation.document_snapshot(circuit), selection_); placement_preview_.reset(placement_);
                placing_ = !placement_.cells.empty(); status_ = "CLICK TO PLACE A DUPLICATE"; break;
            case SDLK_I: {
                std::set<Point> points;
                for (const auto& cell : circuit.cells()) if (!selection_.contains(cell.position)) points.insert(cell.position);
                selection_ = Selection::points(std::move(points)); selection_changed_ = false; break;
            }
            case SDLK_SPACE: edit_speed(); break;
            default: break;
            }
            return;
        }
        if (e.key >= SDLK_0 && e.key <= SDLK_9) {
            cancel_gesture();
            const auto digit = static_cast<std::size_t>(e.key - SDLK_0);
            tools_[0] = {ToolKind::pencil, palette[(digit + 9) % 10]}; placing_ = false; return;
        }
        switch (e.key) {
        case SDLK_SPACE: running = !running; accumulator_ = 0; break;
        case SDLK_RIGHT:
        case SDLK_F10: running = false; tick(); break;
        case SDLK_F9:
            cancel_gesture(); keyboard_focus_.reset();
            if (keyboard_cursor_) { keyboard_cursor_.reset(); status_ = "POINTER NAVIGATION"; }
            else {
                keyboard_cursor_ = hover_.value_or(view.cell(760, 430).value_or(Point{}));
                hover_ = keyboard_cursor_; selection_.clear(); view.center_on(*keyboard_cursor_);
                status_ = "KEYBOARD CANVAS - ARROWS: NAVIGATE, F8: INSPECT, F10: STEP, ESC: EXIT";
            }
            break;
        case SDLK_R: reset_simulation(); break;
        case SDLK_Q: cancel_gesture(); tools_[0] = {ToolKind::selector}; placing_ = false; break;
        case SDLK_E: cancel_gesture(); eyedropper_ = true; break;
        case SDLK_I: cancel_gesture(); tools_[0] = {ToolKind::interactor}; placing_ = false; break;
        case SDLK_F5: cancel_gesture(); tools_[0] = {ToolKind::pencil, Element::screen}; placing_ = false; break;
        case SDLK_F6: cancel_gesture(); tools_[0] = {ToolKind::pencil, Element::file_input}; placing_ = false; break;
        case SDLK_F8:
            if (hover_) {
                cancel_gesture(); accumulator_ = 0;
                if (!inspection_dialog_(window_, describe_cell(circuit, simulation, *hover_))) status_ = SDL_GetError();
                discard_elapsed_ = true;
            } else status_ = "POINT AT A CELL, THEN PRESS F8 TO INSPECT";
            break;
        case SDLK_F7: cancel_gesture(); tools_[0] = {ToolKind::pencil, Element::file_output}; placing_ = false; break;
        case SDLK_B: beginner_ = !beginner_; status_ = beginner_ ? "BEGINNER HINTS ON" : "BEGINNER HINTS OFF"; break;
        case SDLK_F2: cancel_gesture(); help_ = !help_; break;
        case SDLK_F3: cancel_gesture(); help_ = false; examples_menu_ = true; break;
        case SDLK_F4: show_recovery(); break;
        case SDLK_F1: {
            const auto executable = current_executable();
            const auto local = executable ? manual_path(*executable) : std::nullopt;
            const auto uri = local ? file_uri(*local) : std::expected<std::string, std::string>(std::string("https://github.com/michaelbawuah/Gatehaven/blob/main/docs/manual.md"));
            if (!uri) status_ = uri.error();
            else if (!SDL_OpenURL(uri->c_str())) status_ = SDL_GetError();
            break;
        }
        case SDLK_F: view.frame(circuit.bounds()); break;
        case SDLK_HOME: view.frame(circuit.bounds()); break;
        case SDLK_EQUALS:
        case SDLK_PLUS:
        case SDLK_KP_PLUS: view.zoom(1.25, 760, 430); break;
        case SDLK_MINUS:
        case SDLK_KP_MINUS: view.zoom(0.8, 760, 430); break;
        case SDLK_D:
        case SDLK_DELETE:
        case SDLK_BACKSPACE: erase_selection(); break;
        case SDLK_H: transform('h'); break;
        case SDLK_V: transform('v'); break;
        case SDLK_LEFTBRACKET: transform('l'); break;
        case SDLK_RIGHTBRACKET: transform('r'); break;
        default: break;
        }
    }

    void undo(bool redo) {
        cancel_gesture();
        if (redo ? history.redo(circuit) : history.undo(circuit)) {
            selection_.clear(); placing_ = false;
             simulation.invalidate(history.last_changes());
            simulation.refresh(circuit);
            endpoints_.prune(circuit);
            status_ = redo ? "REDONE" : "UNDONE";
        }
    }

    void tick() {
        endpoints_.prune(circuit);
        simulation.step(circuit, [&](const CommunicatorGroup& group, bool sending) {
            const bool received = endpoints_.exchange(group, sending);
            for (const auto point : group.cells) {
                const auto error = endpoints_.error(point);
                if (!error.empty()) status_ = "FILE PORT: " + error;
            }
            return received;
        });
    }
    void reset_simulation() { simulation.initialize(circuit, true); endpoints_.reset_protocols(); accumulator_ = 0; }

    void polyline_preview(Point target) {
        const auto result = polyline_->preview(target, view.visible());
        if (result) preview_ = *result;
        else { preview_.clear(); status_ = result.error(); }
    }

    void edit_speed() {
        speed_edit_ = std::to_string(speed_);
        speed_replace_ = true;
        speed_error_ = false;
        accumulator_ = 0;
        cancel_gesture();
    }

    void speed_key(SDL_Keycode code) {
        if (code == SDLK_ESCAPE) { speed_edit_.reset(); return; }
        if (code == SDLK_RETURN || code == SDLK_KP_ENTER) { commit_speed(); return; }
        if (code == SDLK_BACKSPACE || code == SDLK_DELETE) {
            if (speed_replace_) speed_edit_->clear();
            else if (!speed_edit_->empty()) speed_edit_->pop_back();
            speed_replace_ = false; speed_error_ = false; return;
        }
        std::optional<char> digit;
        if (code >= SDLK_0 && code <= SDLK_9) digit = static_cast<char>('0' + code - SDLK_0);
        else if (code == SDLK_KP_0) digit = '0';
        else if (code >= SDLK_KP_1 && code <= SDLK_KP_9) digit = static_cast<char>('1' + code - SDLK_KP_1);
        if (digit) {
            if (speed_replace_) speed_edit_->clear();
            if (speed_edit_->size() < 4) speed_edit_->push_back(*digit);
            speed_replace_ = false; speed_error_ = false;
        }
    }

    void commit_speed() {
        unsigned value{};
        const auto result = std::from_chars(speed_edit_->data(), speed_edit_->data() + speed_edit_->size(), value);
        if (result.ec != std::errc{} || result.ptr != speed_edit_->data() + speed_edit_->size() || value < 1 || value > 1000) {
            speed_error_ = true; return;
        }
        speed_ = value;
        speed_edit_.reset();
        accumulator_ = 0;
        status_ = "SIMULATION SPEED: " + std::to_string(speed_) + " TICKS/S";
    }

    void delete_recovery() {
        if (!recovery_ || recovery_index_ >= recovery_entries_.size()) return;
        const auto removed = recovery_->remove(recovery_entries_[recovery_index_].id);
        recovery_delete_ = false;
        if (!removed) { status_ = "RECOVERY: " + removed.error(); return; }
        recovery_entries_.erase(recovery_entries_.begin() + static_cast<std::ptrdiff_t>(recovery_index_));
        if (recovery_index_ >= recovery_entries_.size()) recovery_index_ = recovery_entries_.empty() ? 0 : recovery_entries_.size() - 1;
        status_ = "ABANDONED SNAPSHOT DELETED";
    }

    void restore_recovery(std::size_t index) {
        if (!recovery_ || index >= recovery_entries_.size()) return;
        if (history.modified()) { status_ = "SAVE THIS CIRCUIT OR OPEN A NEW WINDOW BEFORE RECOVERING"; return; }
        auto restored = recovery_->restore(recovery_entries_[index].id);
        if (!restored) { status_ = "RECOVERY: " + restored.error(); return; }
        cancel_gesture(); circuit = std::move(*restored); path_.clear();
        history.clear(); history.mark_unsaved(); simulation.initialize(circuit); endpoints_.clear();
        selection_.clear(); placing_ = false; running = false; recovery_menu_ = false;
        recovery_dirty_ = true; recovery_schedule_.written(circuit.revision());
        view.frame(circuit.bounds()); status_ = "CIRCUIT RECOVERED - SAVE TO KEEP YOUR WORK";
    }

    static std::string recovery_date(std::filesystem::file_time_type time) {
        const auto stamp = system_time(time);
        const auto day = std::chrono::floor<std::chrono::days>(stamp);
        const std::chrono::year_month_day date{day};
        const std::chrono::hh_mm_ss clock{std::chrono::floor<std::chrono::seconds>(stamp - day)};
        const auto pad = [](auto value) { auto text = std::to_string(value); return text.size() == 1 ? "0" + text : text; };
        return std::to_string(static_cast<int>(date.year())) + "-" + pad(static_cast<unsigned>(date.month())) + "-" +
            pad(static_cast<unsigned>(date.day())) + " " + pad(clock.hours().count()) + ":" + pad(clock.minutes().count()) + " UTC";
    }

    static ViewRect recovery_button(std::size_t row) { return {390, 270 + static_cast<double>(row) * 54, 500, 42}; }
    void render_recovery(SDL_Renderer* r) const {
        rectangle(r, 350, 180, 580, 460, ink);
        ui::text(r, 390, 212, "RECOVER UNSAVED CIRCUITS", white, 2);
        if (recovery_delete_) {
            ui::text(r, 390, 306, "DELETE THIS SNAPSHOT PERMANENTLY?", white, 1.5F);
            ui::text(r, 390, 346, "YOUR SAVED CIRCUIT IS NOT AFFECTED.", white, 1.5F);
            ui::text(r, 390, 402, "ENTER: DELETE     ESC: KEEP", white, 1.5F);
            return;
        }
        if (recovery_entries_.empty()) ui::text(r, 390, 294, "NO ABANDONED SNAPSHOTS", white, 1.5F);
        const auto start = (recovery_index_ / 5) * 5;
        for (std::size_t i = start; i < std::min(start + 5, recovery_entries_.size()); ++i) {
            const auto box = recovery_button(i - start);
            rectangle(r, static_cast<float>(box.x), static_cast<float>(box.y), static_cast<float>(box.width), static_cast<float>(box.height), i == recovery_index_ ? teal : muted);
            const auto label = recovery_date(recovery_entries_[i].modified) + "  " + std::to_string((recovery_entries_[i].bytes + 1023) / 1024) + " KB";
            ui::text(r, 406, static_cast<float>(box.y + 14), label, white, 1.5F);
        }
        if (!recovery_entries_.empty()) ui::text(r, 390, 544,
            std::to_string(recovery_index_ + 1) + " OF " + std::to_string(recovery_entries_.size()) + " SNAPSHOTS", white, 1.25F);
        ui::text(r, 390, 566, "UP/DOWN: CHOOSE   ENTER: RECOVER", white, 1.25F);
        ui::text(r, 390, 592, "DEL: DELETE   ESC: KEEP FOR LATER", white, 1.25F);
    }

    void render_speed_dialog(SDL_Renderer* r) const {
        rectangle(r, 410, 234, 470, 306, ink);
        ui::text(r, 452, 272, "SIMULATION SPEED", white, 2.5F);
        ui::text(r, 452, 314, "TICKS PER SECOND: 1 TO 1000", white, 1.5F);
        rectangle(r, 484, 350, 320, 58, speed_replace_ ? teal : white);
        ui::text(r, 508, 366, speed_edit_->empty() ? "_" : *speed_edit_, speed_replace_ ? white : ink, 3);
        if (speed_error_) ui::text(r, 452, 427, "ENTER A WHOLE NUMBER FROM 1 TO 1000", {255, 182, 148, 255}, 1.25F);
        else ui::text(r, 452, 427, "ENTER: APPLY    ESC: CANCEL", white, 1.5F);
        rectangle(r, 484, 460, 148, 40, muted);
        rectangle(r, 656, 460, 148, 40, teal);
        ui::text(r, 520, 473, "CANCEL", white, 2);
        ui::text(r, 696, 473, "APPLY", white, 2);
    }

    void copy(bool cut) {
        if (!selection_) { status_ = "SELECT A REGION FIRST"; return; }
        const auto result = clipboards_.write(clipboard_, capture_selection(simulation.document_snapshot(circuit), selection_));
        if (!result) { status_ = "COPY FAILED: " + result.error(); return; }
        if (cut) erase_selection();
        status_ = "COPIED TO SHARED CLIPBOARD " + std::to_string(clipboard_);
    }

    void clipboard_action(char action, bool choose) {
        if (action != 'v' && !selection_) { status_ = "SELECT A REGION FIRST"; return; }
        cancel_gesture(); placing_ = false;
        if (choose) { clipboard_menu_ = action; return; }
        clipboard_ = 0;
        if (action == 'v') begin_paste(); else copy(action == 'x');
    }

    void choose_clipboard(unsigned slot) {
        const auto action = *clipboard_menu_;
        clipboard_menu_.reset();
        clipboard_ = slot;
        if (action == 'v') begin_paste(); else copy(action == 'x');
    }

    static ViewRect clipboard_button(unsigned slot) {
        return {376.0 + static_cast<double>(slot % 5) * 108,
                328.0 + static_cast<double>(slot / 5) * 70, 96, 54};
    }

    void render_clipboard_menu(SDL_Renderer* r) const {
        rectangle(r, 340, 234, 600, 288, ink);
        const std::string action = *clipboard_menu_ == 'v' ? "PASTE FROM" : *clipboard_menu_ == 'x' ? "CUT TO" : "COPY TO";
        ui::text(r, 376, 266, action + " SHARED CLIPBOARD", white, 2);
        ui::text(r, 376, 300, "0 FOLLOWS THE LAST CLIPBOARD USED", {170, 208, 196, 255}, 1.5F);
        for (unsigned slot = 0; slot < 10; ++slot) {
            const auto box = clipboard_button(slot);
            rectangle(r, static_cast<float>(box.x), static_cast<float>(box.y), 96, 54,
                      slot == clipboard_ ? teal : muted);
            ui::text(r, static_cast<float>(box.x + 39), static_cast<float>(box.y + 15), std::to_string(slot), white, 3);
        }
        ui::text(r, 376, 480, "PRESS 0-9 OR CLICK A SLOT. ESC: CANCEL", white, 1.5F);
    }

    void begin_paste() {
        const auto stamp = clipboards_.read(clipboard_);
        placing_ = false;
        if (!stamp) { status_ = "PASTE FAILED: " + stamp.error(); return; }
        placement_ = *stamp; // Keep a stable preview if another window changes this slot.
        placement_preview_.reset(placement_);
        placing_ = !placement_.cells.empty();
        status_ = placing_ ? "CLICK TO PLACE - BRACKETS ROTATE" : "CLIPBOARD IS EMPTY";
    }

    void erase_selection() {
        if (!selection_) return;
        auto edits = selection_.cells(circuit);
        for (auto& cell : edits) { cell.element = Element::empty; cell.state = 0; }
        if (apply(edits)) selection_.clear();
    }

    void transform(char operation) {
        if (!placing_ && !selection_) return;
        auto stamp = placing_ ? placement_ : capture_selection(simulation.document_snapshot(circuit), selection_);
        if (operation == 'h') stamp.flip_horizontal();
        else if (operation == 'v') stamp.flip_vertical();
        else {
            const auto rotations = operation == 'l' ? 3 : 1;
            for (int i = 0; i < rotations; ++i) stamp.rotate_clockwise();
        }
        if (placing_) { placement_ = std::move(stamp); placement_preview_.reset(placement_); return; }
        const auto result = place_selection(circuit, selection_, stamp, selection_.bounds()->min);
        if (!result) { status_ = result.error(); return; }
        if (apply(result->edits)) { selection_ = result->selection; selection_changed_ = true; }
    }

    bool discard_changes() {
        if (!history.modified()) return true;
        cancel_gesture();
        running = false;
        const SDL_MessageBoxButtonData choices[]{
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"},
            {0, 1, "Discard changes"}, {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 2, "Save"}};
        const SDL_MessageBoxData data{SDL_MESSAGEBOX_WARNING, window_, "Unsaved circuit",
            "Save your changes before closing this circuit?", 3, choices, nullptr};
        int selected = 0;
        if (!SDL_ShowMessageBox(&data, &selected)) return false;
        if (selected == 1) return true;
        if (selected != 2) return false;
        if (!path_.empty()) return save(path_);
        close_after_save_ = true;
        file_dialog(true);
        return false;
    }

    void launch(std::optional<std::filesystem::path> document) {
        const auto result = launcher_(std::move(document));
        status_ = result ? "OPENED IN A NEW GATEHAVEN WINDOW" : "NEW WINDOW FAILED: " + result.error();
    }

    void fresh() { launch(std::nullopt); }

    bool save(const std::filesystem::path& path) {
        if (path == path_ && !path_.empty()) {
            const auto current = fingerprint_file(path);
            if (!current) { status_ = current.error(); return false; }
            if (*current != disk_version_) {
                if (!confirm_overwrite_(window_)) { status_ = "SAVE CANCELED - CTRL SHIFT S KEEPS BOTH VERSIONS"; return false; }
            }
        }
        const auto result = save_document(path, simulation.document_snapshot(circuit));
        if (!result) { status_ = result.error().message; return false; }
        path_ = path;
        const auto inspected = fingerprint_file(path);
        disk_version_ = inspected ? *inspected : std::nullopt;
        history.mark_saved(); clear_checkpoint();
        status_ = document_format(path) == DocumentFormat::legacy
            ? "LEGACY CIRCUIT SAVED - FILE ORIGIN IS TOP LEFT" : "CIRCUIT SAVED";
        return true;
    }

    void request_save(bool save_as) {
        if (save_as || path_.empty()) file_dialog(true);
        else save(path_);
    }

    void file_dialog(bool saving) {
        if (dialog_pending_) return;
        cancel_gesture();
        running = false;
        dialog_pending_ = true;
        static const SDL_DialogFileFilter save_filters[]{{"Gatehaven circuit", "ghv"}, {"Legacy circuit", "ccsb"}};
        static const SDL_DialogFileFilter open_filters[]{{"Circuit files", "ghv;ccsb"}, {"All files", "*"}};
        const auto utf8 = path_.empty() ? std::u8string(u8"circuit.ghv") : path_.u8string();
        auto request = std::make_unique<DialogRequest>(DialogRequest{mailbox_, saving, {utf8.begin(), utf8.end()}, std::nullopt});
        const auto location = request->location.c_str();
        if (saving) SDL_ShowSaveFileDialog(dialog_callback, request.release(), window_, save_filters, 2, location);
        else SDL_ShowOpenFileDialog(dialog_callback, request.release(), window_, open_filters, 2, nullptr, false);
    }

    void communicator_dialog(Point point) {
        if (dialog_pending_) return;
        cancel_gesture(); running = false; dialog_pending_ = true;
        const bool output = circuit.at(point) == Element::file_output;
        for (const auto& group : communicator_groups(circuit)) {
            if (std::find(group.cells.begin(), group.cells.end(), point) != group.cells.end()) { point = group.id; break; }
        }
        auto request = std::make_unique<DialogRequest>(DialogRequest{mailbox_, output, "output.bin", point});
        const auto location = request->location.c_str();
        if (output) SDL_ShowSaveFileDialog(dialog_callback, request.release(), window_, nullptr, 0, location);
        else SDL_ShowOpenFileDialog(dialog_callback, request.release(), window_, nullptr, 0, nullptr, false);
    }

    void draw_bindings(SDL_Renderer* r, float y, InputTool tool) const {
        for (std::size_t i = 0; i < tools_.size(); ++i) {
            if (tools_[i] == tool) rectangle(r, 182 + static_cast<float>(i) * 7, y, 5, 14, binding_colors[i]);
        }
    }

    void draw_selection(SDL_Renderer* r, Bounds region) const {
        const auto [x, y] = view.screen(region.min);
        const double w = (static_cast<double>(region.max.x) - region.min.x + 1) * view.scale;
        const double h = (static_cast<double>(region.max.y) - region.min.y + 1) * view.scale;
        // Clip geometry in double precision before passing float rectangles to SDL.
        const auto left = std::clamp(x, 240.0, 1280.0);
        const auto top = std::clamp(y, 96.0, 764.0);
        const auto right = std::clamp(x + w, 240.0, 1280.0);
        const auto bottom = std::clamp(y + h, 96.0, 764.0);
        rectangle(r, static_cast<float>(left), static_cast<float>(top), static_cast<float>(right - left),
                  static_cast<float>(bottom - top), {61, 112, 195, 255}, true);
    }

    void draw_cell(SDL_Renderer* r, Cell cell, std::uint8_t ports, bool preview = false) const {
        const auto [wx, wy] = view.screen(cell.position);
        const float x = static_cast<float>(wx), y = static_cast<float>(wy);
        const float s = static_cast<float>(view.scale);
        const float center = s / 2;
        const float stroke = std::max(2.0F, s * 0.13F);
        const auto powered = ports != 0;
        const auto color = preview ? orange : powered ? teal : SDL_Color{99, 117, 128, 255};
        if (cell.element == Element::empty) {
            rectangle(r, x + 1, y + 1, s - 2, s - 2, {240, 186, 174, 255}); return;
        }
        rectangle(r, x + 1, y + 1, s - 2, s - 2,
                  powered ? SDL_Color{217, 238, 227, 255} : SDL_Color{233, 238, 241, 255});
        if (cell.element == Element::screen) {
            const bool bright = !preview && simulation.sent(cell.position);
            rectangle(r, x + 3, y + 3, std::max(1.0F, s - 6), std::max(1.0F, s - 6),
                      bright ? SDL_Color{246, 190, 65, 255} : SDL_Color{57, 64, 84, 255});
            if (s >= 18) ui::text(r, x + s / 2 - 3, y + s / 2 - 3.5F, "S", bright ? ink : white, 1);
            if (preview) rectangle(r, x, y, s, s, orange, true);
            return;
        }
        if (cell.element == Element::wire || cell.element == Element::crossing || cell.element == Element::signal) {
            const auto horizontal = (ports & 10) != 0 ? teal : SDL_Color{99, 117, 128, 255};
            const auto vertical = (ports & 5) != 0 ? teal : SDL_Color{99, 117, 128, 255};
            const auto connected = [&](Direction direction) {
                const auto next = neighbor(cell.position, direction);
                return preview || (next && circuit.at(*next) != Element::empty);
            };
            const bool east = connected(Direction::east), west = connected(Direction::west);
            const bool north = connected(Direction::north), south = connected(Direction::south);
            if (west) rectangle(r, x, y + center - stroke / 2, center + stroke / 2, stroke, preview ? orange : horizontal);
            if (east) rectangle(r, x + center - stroke / 2, y + center - stroke / 2,
                                center + stroke / 2, stroke, preview ? orange : horizontal);
            if (cell.element == Element::crossing && (north || south)) {
                rectangle(r, x + center - stroke, y, stroke * 2, s, paper);
            }
            if (north) rectangle(r, x + center - stroke / 2, y, stroke, center + stroke / 2, preview ? orange : vertical);
            if (south) rectangle(r, x + center - stroke / 2, y + center - stroke / 2,
                                 stroke, center + stroke / 2, preview ? orange : vertical);
            if (!north && !south && !east && !west) {
                rectangle(r, x + center - stroke, y + center - stroke, stroke * 2, stroke * 2, color);
            }
            if (cell.element == Element::signal) {
                rectangle(r, x + s * 0.3F, y + s * 0.3F, s * 0.4F, s * 0.4F,
                          powered ? orange : SDL_Color{161, 110, 66, 255});
            }
        } else {
            const float padding = std::min(3.0F, s / 8);
            rectangle(r, x + padding, y + padding, s - 2 * padding, s - 2 * padding,
                      cell.element == Element::source ? orange : powered ? teal : SDL_Color{91, 89, 130, 255});
            const auto label = cell.element == Element::file_input ? std::string_view("IN") :
                               cell.element == Element::file_output ? std::string_view("OUT") :
                               cell.element == Element::source ? std::string_view("+") :
                               cell.element == Element::positive_relay ? std::string_view("+R") :
                               cell.element == Element::negative_relay ? std::string_view("-R") : name(cell.element);
            if (s >= 18) {
                const float font = std::min(1.5F, (s - 8) / (static_cast<float>(label.size()) * 6));
                ui::text(r, x + center - static_cast<float>(label.size()) * 3 * font,
                         y + center - 3.5F * font, label, white, font);
            }
        }
        if (preview) rectangle(r, x, y, s, s, color, true);
    }

    static ViewRect example_button(std::size_t index) { return {406, 252 + static_cast<double>(index) * 54, 548, 42}; }

    void choose_example(std::size_t index) {
        examples_menu_ = false;
        const auto result = demo_launcher_(example_names[index]);
        status_ = result ? "EXAMPLE OPENED IN A NEW WINDOW" : "EXAMPLE FAILED: " + result.error();
    }

    void render_examples(SDL_Renderer* r) const {
        rectangle(r, 374, 162, 612, 478, ink);
        ui::text(r, 406, 194, "EXPLORE A CIRCUIT", white, 2.5F);
        ui::text(r, 406, 226, "OPENS IN A NEW WINDOW", {170, 208, 196, 255}, 1.5F);
        for (std::size_t i = 0; i < example_names.size(); ++i) {
            const auto box = example_button(i);
            rectangle(r, 406, static_cast<float>(box.y), 548, 42, i == example_index_ ? teal : muted);
            ui::text(r, 428, static_cast<float>(box.y + 13), std::to_string(i + 1) + "  " + std::string(example_names[i]), white, 2);
        }
        ui::text(r, 406, 602, "1-6 OR ARROWS + ENTER. ESC: CLOSE", white, 1.5F);
    }

    void render_hint(SDL_Renderer* r) const {
        std::string_view first = "DRAG TO DRAW A STRAIGHT LINE. HOLD SHIFT TO CHAIN LINES.";
        std::string_view second = "BACKSPACE RETRACES A POLYLINE. DOUBLE CLICK FINISHES IT.";
        if (placing_) { first = "CLICK TO PLACE YOUR COPIED CIRCUIT. ESC CANCELS."; second = "[ AND ] ROTATE. H AND V FLIP THE PREVIEW."; }
        else if (eyedropper_) { first = "CLICK A CELL TO BIND THAT BUTTON TO ITS PENCIL."; second = "CLICK EMPTY SPACE TO BIND THE ERASER."; }
        else if (tools_[0].kind == ToolKind::selector) {
            first = "SHIFT ADDS TO THE SELECTION. ALT REMOVES FROM IT.";
            second = "DOUBLE CLICK: ELECTRICAL NET. TRIPLE CLICK: WHOLE CIRCUIT.";
        } else if (tools_[0].kind == ToolKind::panner) {
            first = "DRAG TO MOVE THE CAMERA. SCROLL TO ZOOM AT THE CURSOR.";
            second = "DOUBLE CLICK CENTERS A CELL. F FRAMES THE WHOLE CIRCUIT.";
        } else if (tools_[0].kind == ToolKind::eraser) {
            first = "DRAG TO ERASE A LINE. SHIFT CHAINS ERASER SEGMENTS.";
            second = "CTRL Z RESTORES THE WHOLE EDIT.";
        } else if (tools_[0].kind == ToolKind::interactor) {
            first = "HOLD A SCREEN TO SEND POWER INTO THE CIRCUIT.";
            second = "CLICK A FILE PORT TO CHOOSE ITS INPUT OR OUTPUT FILE.";
        } else if (tools_[0].element == Element::screen) {
            first = "ADJACENT SCREENS WORK AS ONE. I: HOLD TO INTERACT.";
            second = "BRIGHTNESS SHOWS SIGNALS SENT BY THE CIRCUIT.";
        } else if (is_communicator(tools_[0].element)) {
            first = "FILE PORTS TRANSFER SERIAL BITS, ONE PER TICK.";
            second = "I: CHOOSE A FILE. F1: PROTOCOL GUIDE IN THE MANUAL.";
        }
        rectangle(r, 254, 690, 674, 60, white);
        ui::text(r, 268, 703, first, teal, 1.25F);
        ui::text(r, 268, 727, second, muted, 1.25F);
    }

    void render_help(SDL_Renderer* r) const {
        rectangle(r, 338, 132, 846, 608, ink);
        ui::text(r, 376, 170, "BUILD YOUR FIRST CIRCUIT", white, 2.5F);
        constexpr std::array<std::string_view, 22> lines{
            "1-0: COMPONENTS      Q: SELECT REGION",
            "F5/F6/F7: SCREEN / FILE IN / FILE OUT",
            "I: INTERACT WITH SCREENS AND FILE PORTS",
            "CLICK TOOL: BIND THAT MOUSE BUTTON",
            "HOLD E + CLICK: SAMPLE A TOOL",
            "SHIFT + DRAW: POLYLINE; BACKSPACE: RETRACE",
            "SELECT: SHIFT ADDS, ALT SUBTRACTS",
            "SPACE: PLAY/PAUSE    RIGHT/F10: ONE TICK",
            "CTRL SPACE: SET TICKS PER SECOND",
            "R: RESET            F: FRAME CIRCUIT",
            "CTRL C/X/V: COPY / CUT / PASTE",
            "CTRL Z/Y: UNDO / REDO    CTRL D: DUPLICATE",
            "CTRL S/O/N: SAVE / OPEN / NEW",
            "[ AND ]: ROTATE     H/V: FLIP",
            "ARROWS: MOVE SELECTION  CTRL: X4",
            "CTRL SHIFT C/V: CHOOSE CLIPBOARD",
            "F1: MANUAL   F3: EXAMPLES   F4: RECOVERY",
            "TAB: FOCUS CONTROLS. ENTER: ACTIVATE",
            "F9: KEYBOARD CANVAS. ARROWS: NAVIGATE",
            "ENTER: USE TOOL. F8: INSPECT CELL",
            "TWO FINGERS: PAN AND PINCH TO ZOOM",
            "F2 OR ESC: CLOSE    B: BEGINNER HINTS"};
        for (std::size_t i = 0; i < lines.size(); ++i) {
            ui::text(r, 378, 208 + static_cast<float>(i) * 23, lines[i], white, 1.75F);
        }
    }
};

void dispatch(App& app, SDL_Renderer* renderer) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        SDL_ConvertEventToRenderCoordinates(renderer, &event);
        app.event(event);
    }
}

void self_test(App& app, SDL_Renderer* renderer, const std::filesystem::path& session_directory,
               const std::vector<std::optional<std::filesystem::path>>& launched, const std::vector<std::string>& demos, const std::vector<std::string>& inspections) {
    const auto require = [](bool ok, const char* message) {
        if (!ok) throw std::runtime_error(message);
    };
    const auto key = [&](SDL_Keycode code, SDL_Keymod mod = SDL_KMOD_NONE) {
        SDL_Event event{}; event.type = SDL_EVENT_KEY_DOWN;
        event.key.key = code; event.key.mod = mod;
        require(SDL_PushEvent(&event), "Could not push keyboard event");
        dispatch(app, renderer);
    };
    key(SDLK_TAB); key(SDLK_RETURN);
    require(app.running, "Keyboard focus could not activate Play");
    key(SDLK_RETURN); require(!app.running, "Keyboard focus could not activate Pause");
    key(SDLK_ESCAPE);
    key(SDLK_TAB, SDL_KMOD_SHIFT); // Last tool is Interactor.
    key(SDLK_RETURN); key(SDLK_1);
    require(!app.history.modified(), "Keyboard palette navigation edited the circuit");
    const auto original = app.circuit;
    key(SDLK_F3); key(SDLK_DOWN); key(SDLK_RETURN);
    require(demos.size() == 1 && demos.back() == "oscillator", "Example chooser did not launch the selected lesson");
    key(SDLK_F3); key(SDLK_ESCAPE);
    require(demos.size() == 1 && app.circuit == original, "Example chooser changed the active document");
    key(SDLK_SPACE);
    require(app.running, "Play event failed");
    app.update(std::numeric_limits<double>::quiet_NaN());
    app.update(std::numeric_limits<double>::infinity());
    app.update(-1);
    require(app.simulation.ticks() == 0, "Invalid elapsed time advanced the simulation");
    app.update(0.21);
    require(app.simulation.ticks() == 1, "Clock did not step");
    key(SDLK_SPACE);
    key(SDLK_RIGHT);
    require(!app.running && app.simulation.ticks() == 2, "Pause/step failed");
    require(app.simulation.powered({8, 0}), "Starter AND output was not powered");
    const auto pointer = [&](Uint32 type, float x, float y, Uint8 button, SDL_MouseID device) {
        SDL_Event event{}; event.type = type; event.button.button = button; event.button.which = device;
        event.button.x = x; event.button.y = y;
        require(SDL_PushEvent(&event), "Could not push mouse event");
        dispatch(app, renderer);
    };
    const auto mouse = [&](Uint32 type, Point cell, Uint8 button = SDL_BUTTON_LEFT, SDL_MouseID device = 0) {
        const auto [x, y] = app.view.screen(cell);
        pointer(type, static_cast<float>(x + app.view.scale / 2), static_cast<float>(y + app.view.scale / 2), button, device);
    };
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {-5, -4});
    pointer(SDL_EVENT_MOUSE_BUTTON_UP, 50, 50, SDL_BUTTON_LEFT, 0);
    require(app.circuit == original && !app.history.modified(), "Outside release committed a stroke");
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {-5, -4});
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, {-1, -4});
    require(app.circuit.size() == original.size() + 5, "Mouse stroke failed");
    key(SDLK_Z, SDL_KMOD_CTRL);
    require(app.circuit == original, "Keyboard undo failed");
    key(SDLK_Y, SDL_KMOD_CTRL);
    require(app.circuit.size() == original.size() + 5, "Keyboard redo failed");
    key(SDLK_R);
    require(app.simulation.ticks() == 0, "Reset failed");
    key(SDLK_SPACE, SDL_KMOD_CTRL);
    key(SDLK_KP_2); key(SDLK_KP_3); key(SDLK_KP_ENTER);
    key(SDLK_SPACE);
    app.update(0.1);
    require(app.simulation.ticks() == 2, "Custom simulation speed was not applied");
    key(SDLK_SPACE, SDL_KMOD_CTRL);
    key(SDLK_0); key(SDLK_RETURN);
    app.update(0.2);
    require(app.simulation.ticks() == 2, "Invalid speed closed the dialog or advanced simulation");
    key(SDLK_ESCAPE);
    app.update(0.1);
    require(app.simulation.ticks() == 4, "Cancel changed the previous simulation speed");
    key(SDLK_SPACE); key(SDLK_R);
    const auto edited = app.circuit;
    key(SDLK_N, SDL_KMOD_CTRL);
    require(launched.size() == 1 && !launched[0], "New did not request a separate instance");
    require(app.circuit == edited, "New replaced the current unsaved circuit");
    const auto open_path = session_directory / "circuit with spaces.ghv";
    app.launch_open(open_path);
    require(launched.size() == 2 && launched[1] == open_path, "Open did not request a separate instance");
    require(app.circuit == edited, "Open replaced the current unsaved circuit");
    const auto dropped_utf8 = open_path.u8string();
    const std::string dropped_path(dropped_utf8.begin(), dropped_utf8.end());
    SDL_Event dropped{}; dropped.type = SDL_EVENT_DROP_FILE; dropped.drop.data = dropped_path.c_str();
    require(SDL_PushEvent(&dropped), "Could not queue a dropped circuit"); dispatch(app, renderer);
    require(launched.size() == 3 && launched.back() == open_path && app.circuit == edited,
            "Dropping a circuit did not open a separate document");
    auto peer = ClipboardSession::join(session_directory);
    require(peer.has_value(), "Could not join shared clipboard for editor test");
    key(SDLK_A, SDL_KMOD_CTRL);
    key(SDLK_C, static_cast<SDL_Keymod>(SDL_KMOD_CTRL | SDL_KMOD_SHIFT));
    key(SDLK_3);
    const auto copied = (*peer)->read(3);
    require(copied && copied->cells.size() == edited.size(), "Editor copy was not shared");
    const auto blocked_slot = session_directory / "slot-4.ghclip";
    std::filesystem::create_directory(blocked_slot);
    key(SDLK_X, static_cast<SDL_Keymod>(SDL_KMOD_CTRL | SDL_KMOD_SHIFT)); key(SDLK_4);
    require(app.circuit == edited, "A failed shared clipboard write deleted the cut selection");
    std::filesystem::remove(blocked_slot);
    const Stamp first{2, 1, {{0, 0, Element::source}, {1, 0, Element::wire}}};
    require((*peer)->write(3, first).has_value(), "Peer copy failed");
    key(SDLK_ESCAPE);
    key(SDLK_V, SDL_KMOD_CTRL);
    require((*peer)->write(3, {1, 1, {{0, 0, Element::nor_gate}}}).has_value(), "Peer overwrite failed");
    key(SDLK_RIGHTBRACKET); // Rotate only the local placement snapshot.
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {13, -4});
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, {13, -4});
    require(app.circuit.at({13, -4}) == Element::source && app.circuit.at({13, -3}) == Element::wire,
            "Shared clipboard changed an active paste preview");
    const auto unchanged = (*peer)->read(3);
    require(unchanged && unchanged->cells[0].element == Element::nor_gate, "Preview transform changed the shared slot");
    key(SDLK_Z, SDL_KMOD_CTRL);
    require(app.circuit == edited, "Pasted stamp did not undo as one edit");
    key(SDLK_Q);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {-5, -4});
    pointer(SDL_EVENT_MOUSE_BUTTON_UP, 50, 50, SDL_BUTTON_LEFT, 0);
    require(app.circuit == edited, "Outside selection release modified the circuit");
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {-5, -4});
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, {-1, -4});
    key(SDLK_RIGHT);
    require(app.circuit.at({-5, -4}) == Element::empty && app.circuit.at({0, -4}) == Element::wire,
            "Right arrow did not move the selection");
    key(SDLK_DOWN, SDL_KMOD_CTRL);
    require(app.circuit.at({0, 0}) == Element::wire && app.circuit.at({0, -4}) == Element::empty,
            "Control-arrow did not move the selection four cells");
    require(app.simulation.ticks() == 0, "Moving a selection accidentally stepped simulation");
    key(SDLK_Z, SDL_KMOD_CTRL); key(SDLK_Z, SDL_KMOD_CTRL);
    require(app.circuit == edited, "Selection moves did not undo cleanly");
    key(SDLK_ESCAPE);
    key(SDLK_Q);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {-5, -4});
    pointer(SDL_EVENT_MOUSE_BUTTON_UP, 50, 50, SDL_BUTTON_LEFT, 0);
    require(app.circuit == edited, "Outside selection release modified the circuit");
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {-5, -4}); mouse(SDL_EVENT_MOUSE_BUTTON_UP, {-4, -4});
    SDL_SetModState(SDL_KMOD_SHIFT);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {-2, -4}); mouse(SDL_EVENT_MOUSE_BUTTON_UP, {-1, -4});
    SDL_SetModState(SDL_KMOD_ALT);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {-4, -4}); mouse(SDL_EVENT_MOUSE_BUTTON_UP, {-4, -4});
    SDL_SetModState(SDL_KMOD_NONE);
    key(SDLK_C, SDL_KMOD_CTRL);
    const auto shaped = (*peer)->read(0);
    require(shaped && shaped->cells.size() == 3 && shaped->width == 5, "Add/subtract selection lost its holes or frame");
    key(SDLK_D, SDL_KMOD_CTRL);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {8, -4}); mouse(SDL_EVENT_MOUSE_BUTTON_UP, {8, -4});
    require(app.circuit.at({8, -4}) == Element::wire && app.circuit.at({9, -4}) == Element::empty &&
            app.circuit.at({11, -4}) == Element::wire && app.circuit.at({12, -4}) == Element::wire,
            "Duplicate did not preserve sparse selected cells");
    const auto after_duplicate = (*peer)->read(0);
    require(after_duplicate && after_duplicate->cells == shaped->cells, "Duplicate changed the shared clipboard");
    key(SDLK_Z, SDL_KMOD_CTRL); key(SDLK_ESCAPE);
    require(app.circuit == edited, "Sparse duplicate did not undo as one edit");
    constexpr std::array<Uint8, 5> inputs{SDL_BUTTON_LEFT, SDL_BUTTON_RIGHT, SDL_BUTTON_MIDDLE, SDL_BUTTON_X1, SDL_BUTTON_X2};
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, 94, 216, inputs[i], 0); // Bind this button to Source.
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, 94, 216, inputs[i], 0);
        const Point target{12, static_cast<Coordinate>(i) - 4};
        mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, target, inputs[i]);
        mouse(SDL_EVENT_MOUSE_BUTTON_UP, target, inputs[i]);
        require(app.circuit.at(target) == Element::source, "A mouse button ignored its tool binding");
    }
    pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, 94, 166, SDL_BUTTON_LEFT, SDL_TOUCH_MOUSEID); // Touch gets Wire.
    pointer(SDL_EVENT_MOUSE_BUTTON_UP, 94, 166, SDL_BUTTON_LEFT, SDL_TOUCH_MOUSEID);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {12, 3}, SDL_BUTTON_LEFT, SDL_TOUCH_MOUSEID);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, {12, 3}, SDL_BUTTON_LEFT, SDL_TOUCH_MOUSEID);
    require(app.circuit.at({12, 3}) == Element::wire, "Touch did not keep its own tool binding");
    key(SDLK_E);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {12, 3}, SDL_BUTTON_X2);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, {12, 3}, SDL_BUTTON_X2);
    SDL_Event released{}; released.type = SDL_EVENT_KEY_UP; released.key.key = SDLK_E;
    require(SDL_PushEvent(&released), "Could not release eyedropper");
    dispatch(app, renderer);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {11, 3}, SDL_BUTTON_X2);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, {11, 3}, SDL_BUTTON_X2);
    require(app.circuit.at({11, 3}) == Element::wire, "Eyedropper did not bind the clicked button");
    for (unsigned i = 0; i < 7; ++i) key(SDLK_Z, SDL_KMOD_CTRL);
    require(app.circuit == edited, "Sampling a tool mutated the document");
    pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, 94, 572, SDL_BUTTON_RIGHT, 0); // Rebind right to Pan.
    pointer(SDL_EVENT_MOUSE_BUTTON_UP, 94, 572, SDL_BUTTON_RIGHT, 0);
    const auto center = app.view.center_x;
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {11, 3}, SDL_BUTTON_RIGHT);
    SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.x = 800; motion.motion.y = 400; motion.motion.xrel = 32; motion.motion.state = SDL_BUTTON_RMASK;
    require(SDL_PushEvent(&motion), "Could not push pan event"); dispatch(app, renderer);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, {11, 3}, SDL_BUTTON_RIGHT);
    require(app.view.center_x < center && app.circuit == edited, "Rebound panner drew or failed to move");
    const auto after_pan = app.view.center_x;
    require(SDL_PushEvent(&motion), "Could not push released pan event"); dispatch(app, renderer);
    require(app.view.center_x == after_pan, "Pan continued after its button was released");
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {11, 3}, SDL_BUTTON_RIGHT);
    key(SDLK_SPACE, SDL_KMOD_CTRL);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, {11, 3}, SDL_BUTTON_RIGHT); // Release consumed by the modal dialog.
    key(SDLK_ESCAPE);
    require(SDL_PushEvent(&motion), "Could not push post-dialog motion"); dispatch(app, renderer);
    require(app.view.center_x == after_pan, "Opening a dialog left a pan gesture active");
    key(SDLK_ESCAPE); key(SDLK_2);
    SDL_SetModState(SDL_KMOD_SHIFT);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {-4, 12}); mouse(SDL_EVENT_MOUSE_BUTTON_UP, {-4, 12});
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {-1, 12}); mouse(SDL_EVENT_MOUSE_BUTTON_UP, {-1, 12});
    require(app.circuit == edited, "A staged polyline modified the circuit before finishing");
    SDL_SetModState(SDL_KMOD_NONE);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {-1, 9}); mouse(SDL_EVENT_MOUSE_BUTTON_UP, {-1, 9});
    require(app.circuit.at({-1, 12}) == Element::wire && app.circuit.at({-1, 10}) == Element::crossing,
            "Desktop polyline failed to connect its insulated corner");
    key(SDLK_Z, SDL_KMOD_CTRL);
    require(app.circuit == edited, "Polyline did not undo as one transaction");
    key(SDLK_Q);
    for (const Uint8 clicks : {Uint8{2}, Uint8{3}}) {
        const auto [x, y] = app.view.screen({-7, -2});
        SDL_Event click{}; click.type = SDL_EVENT_MOUSE_BUTTON_DOWN; click.button.button = SDL_BUTTON_LEFT;
        click.button.clicks = clicks; click.button.x = static_cast<float>(x + app.view.scale / 2);
        click.button.y = static_cast<float>(y + app.view.scale / 2);
        require(SDL_PushEvent(&click), "Could not push connected selection click"); dispatch(app, renderer);
        key(SDLK_C, SDL_KMOD_CTRL);
        const auto selected = (*peer)->read(0);
        require(selected && selected->cells.size() == (clicks == 2 ? 9U : 27U), "Connected selection gesture chose the wrong circuit");
    }
    key(SDLK_ESCAPE);
    key(SDLK_F5);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {10, 3}); mouse(SDL_EVENT_MOUSE_BUTTON_UP, {10, 3});
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {10, 3}); mouse(SDL_EVENT_MOUSE_BUTTON_UP, {10, 3});
    require(app.circuit.at({10, 3}) == Element::signal, "A communicator's own pencil did not place its Signal input");
    key(SDLK_Z, SDL_KMOD_CTRL);
    key(SDLK_1);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {11, 3}); mouse(SDL_EVENT_MOUSE_BUTTON_UP, {11, 3});
    key(SDLK_I);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {10, 3});
    key(SDLK_RIGHT);
    require(app.simulation.powered({11, 3}), "Held screen did not drive the adjacent wire");
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, {10, 3}); key(SDLK_RIGHT);
    require(!app.simulation.powered({11, 3}), "Screen stayed powered after Interact was released");
    key(SDLK_Z, SDL_KMOD_CTRL); key(SDLK_Z, SDL_KMOD_CTRL); key(SDLK_R);
    require(app.circuit == edited, "Screen interaction changed circuit structure");
    const auto pan_before = app.view.center_x;
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {11, 3}, SDL_BUTTON_RIGHT);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, {11, 3}, SDL_BUTTON_RIGHT);
    require(app.view.center_x == pan_before, "Single panner click unexpectedly centered");
    const auto [pan_x, pan_y] = app.view.screen({11, 3});
    SDL_Event double_pan{}; double_pan.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    double_pan.button.button = SDL_BUTTON_RIGHT; double_pan.button.clicks = 2;
    double_pan.button.x = static_cast<float>(pan_x + app.view.scale / 2);
    double_pan.button.y = static_cast<float>(pan_y + app.view.scale / 2);
    require(SDL_PushEvent(&double_pan), "Could not queue double pan");
    double_pan.type = SDL_EVENT_MOUSE_BUTTON_UP;
    require(SDL_PushEvent(&double_pan), "Could not release double pan"); dispatch(app, renderer);
    require(app.view.center_x == 11.5 && app.view.center_y == 3.5, "Double panner click did not center the cell");
    app.view.frame(app.circuit.bounds());
    const auto before_touch = app.circuit;
    const auto finger = [&](Uint32 type, SDL_FingerID id, float x, float y) {
        SDL_Event event{}; event.type = type; event.tfinger.touchID = 1; event.tfinger.fingerID = id;
        float window_x{}, window_y{}; int width{}, height{};
        require(SDL_RenderCoordinatesToWindow(renderer, x, y, &window_x, &window_y), "Could not map touch coordinates");
        require(SDL_GetWindowSize(SDL_GetRenderWindow(renderer), &width, &height), "Could not read touch window size");
        event.tfinger.x = window_x / static_cast<float>(width); event.tfinger.y = window_y / static_cast<float>(height);
        require(SDL_PushEvent(&event), "Could not queue a touch event"); dispatch(app, renderer);
    };
    const auto [touch_x, touch_y] = app.view.screen({9, 4});
    const auto tx = static_cast<float>(touch_x + app.view.scale / 2);
    const auto ty = static_cast<float>(touch_y + app.view.scale / 2);
    finger(SDL_EVENT_FINGER_DOWN, 1, tx, ty); finger(SDL_EVENT_FINGER_UP, 1, tx, ty);
    require(app.circuit.at({9, 4}) == Element::wire, "Native touch did not apply the touch binding");
    key(SDLK_Z, SDL_KMOD_CTRL);
    require(app.circuit == before_touch, "Native touch stroke did not undo once");
    require(SDL_SetWindowSize(SDL_GetRenderWindow(renderer), 1000, 800), "Could not resize touch test window");
    const auto old_scale = app.view.scale;
    finger(SDL_EVENT_FINGER_DOWN, 1, 600, 400); finger(SDL_EVENT_FINGER_DOWN, 2, 800, 400);
    finger(SDL_EVENT_FINGER_MOTION, 2, 900, 400);
    require(app.view.scale > old_scale && app.circuit == before_touch, "Pinch failed or committed its canceled stroke");
    finger(SDL_EVENT_FINGER_UP, 2, 900, 400); finger(SDL_EVENT_FINGER_MOTION, 1, 650, 400);
    finger(SDL_EVENT_FINGER_UP, 1, 650, 400);
    require(app.circuit == before_touch, "Remaining pinch finger resumed drawing");
    finger(SDL_EVENT_FINGER_DOWN, 1, 600, 400);
    SDL_Event lost{}; lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    require(SDL_PushEvent(&lost), "Could not queue focus loss"); dispatch(app, renderer);
    finger(SDL_EVENT_FINGER_UP, 1, 650, 400);
    require(app.circuit == before_touch, "Focus loss committed an unfinished touch stroke");
    key(SDLK_F2); key(SDLK_DELETE); key(SDLK_SPACE);
    const auto help_ticks = app.simulation.ticks(); app.update(0.25);
    require(app.circuit == before_touch && app.simulation.ticks() == help_ticks, "Help allowed hidden editor actions");
    key(SDLK_ESCAPE);
    require(SDL_SetWindowSize(SDL_GetRenderWindow(renderer), 1280, 800), "Could not restore touch test window");
    const auto recovery_directory = session_directory / "recovery";
    auto abandoned = RecoveryStore::open(recovery_directory).value();
    Circuit lost_circuit; lost_circuit.set({42, -17}, Element::source);
    require(abandoned->write(lost_circuit).has_value(), "Could not stage abandoned circuit"); abandoned.reset();
    app.enable_recovery(recovery_directory); app.show_recovery();
    key(SDLK_RETURN);
    require(app.circuit == before_touch, "Recovery replaced a modified circuit");
    key(SDLK_DELETE); key(SDLK_ESCAPE);
    require(app.circuit == before_touch, "Canceled snapshot deletion changed the circuit");
    key(SDLK_ESCAPE); app.history.mark_saved(); app.show_recovery(); key(SDLK_RETURN);
    require(app.circuit == lost_circuit && app.history.modified() && !app.running, "Recovery did not create an unsaved paused document");
    require(!app.history.can_undo(), "Recovery retained the previous document history");
    app.finish_recovery();
    auto disposable = RecoveryStore::open(recovery_directory).value();
    require(disposable->write(lost_circuit).has_value(), "Could not stage deletion test");
    disposable.reset(); app.show_recovery(); key(SDLK_DELETE); key(SDLK_RETURN);
    key(SDLK_ESCAPE);
    auto inspector = RecoveryStore::open(recovery_directory).value();
    require(inspector->scan()->empty(), "Confirmed deletion retained its abandoned snapshot");
    require(app.circuit == lost_circuit, "Deleting a snapshot changed the active document");
    const auto save_path = session_directory / "save workflow.ghv";
    require(save_document(save_path, lost_circuit).has_value() && app.open(save_path), "Could not open save workflow fixture");
    const std::array save_edit{Cell{{43, -17}, Element::wire}};
    require(app.history.apply(app.circuit, save_edit).has_value(), "Could not edit save fixture");
    key(SDLK_S, SDL_KMOD_CTRL);
    require(!app.history.modified() && load_document(save_path).value() == app.simulation.document_snapshot(app.circuit), "Ordinary Save failed");
    const auto legacy_path = session_directory / "legacy workflow.ccsb";
    Circuit legacy_fixture; legacy_fixture.set({0, 0}, Element::positive_relay, 3);
    require(save_document(legacy_path, legacy_fixture).has_value() && app.open(legacy_path), "Legacy open failed");
    key(SDLK_S, SDL_KMOD_CTRL);
    require(load_document(legacy_path).value() == legacy_fixture, "Legacy save lost saved levels");
    const auto moving_path = session_directory / "live selection.ghv";
    Circuit moving;
    moving.set({0, 0}, Element::source); moving.set({1, 0}, Element::wire);
    moving.set({2, 0}, Element::signal); moving.set({3, 0}, Element::or_gate);
    require(save_document(moving_path, moving).has_value() && app.open(moving_path), "Could not load moving-state fixture");
    key(SDLK_RIGHT); key(SDLK_Q);
    require(app.simulation.powered({3, 0}) && app.circuit.saved_state({3, 0}) == 0, "Moving-state fixture did not evolve");
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, {3, 0}); mouse(SDL_EVENT_MOUSE_BUTTON_UP, {3, 0});
    key(SDLK_DOWN);
    require(app.circuit.saved_state({3, 1}) == 2 && app.simulation.powered({3, 1}), "Selection movement lost live output level");
    const auto before_inspection = app.circuit;
    const auto inspection_tick = app.simulation.ticks();
    key(SDLK_F8);
    require(!inspections.empty() && inspections.back().find("Component: empty") != std::string::npos,
            "Native inspector did not report the hovered cell");
    require(app.circuit == before_inspection && app.simulation.ticks() == inspection_tick, "Inspection changed the circuit");
    key(SDLK_Z, SDL_KMOD_CTRL); key(SDLK_ESCAPE);
    require(app.circuit == moving, "Stateful movement did not undo structure and stored levels");
    const auto navigation_tick = app.simulation.ticks();
    key(SDLK_F9); key(SDLK_RIGHT); key(SDLK_DOWN, SDL_KMOD_CTRL); key(SDLK_F8);
    require(inspections.back().starts_with("Cell (4, 4)"), "Keyboard navigation chose the wrong coordinates");
    SDL_Event repeated{}; repeated.type = SDL_EVENT_KEY_DOWN; repeated.key.key = SDLK_DOWN; repeated.key.repeat = true;
    require(SDL_PushEvent(&repeated), "Could not repeat keyboard navigation"); dispatch(app, renderer); key(SDLK_F8);
    require(inspections.back().starts_with("Cell (4, 5)") && app.circuit == moving && app.simulation.ticks() == navigation_tick,
            "Keyboard navigation repeated an edit or advanced the simulation");
    key(SDLK_1); key(SDLK_F); key(SDLK_RETURN);
    require(app.circuit.at({4, 5}) == Element::wire, "Keyboard pencil did not place its component");
    key(SDLK_Q); key(SDLK_RETURN); key(SDLK_C, SDL_KMOD_CTRL);
    require((*peer)->read(0)->cells.size() == 1 && (*peer)->read(0)->cells[0].element == Element::wire,
            "Keyboard selector could not copy a cell");
    key(SDLK_Z, SDL_KMOD_CTRL);
    require(app.circuit == moving, "Keyboard edit did not undo as one action");
    key(SDLK_F5); key(SDLK_RETURN); key(SDLK_I); key(SDLK_RETURN); key(SDLK_F10);
    require(app.simulation.received({4, 5}), "Keyboard interaction did not hold the screen");
    SDL_Event enter_up{}; enter_up.type = SDL_EVENT_KEY_UP; enter_up.key.key = SDLK_RETURN;
    require(SDL_PushEvent(&enter_up), "Could not release keyboard interaction"); dispatch(app, renderer);
    key(SDLK_F10); require(!app.simulation.received({4, 5}), "Keyboard screen remained held after release");
    const auto dialog_tick = app.simulation.ticks(); key(SDLK_SPACE); key(SDLK_F8); app.update(30);
    require(app.simulation.ticks() == dialog_tick, "Time in the native inspector advanced simulation");
    key(SDLK_SPACE);
    key(SDLK_Z, SDL_KMOD_CTRL);
    key(SDLK_ESCAPE);
    const Point edge{std::numeric_limits<Coordinate>::max(), std::numeric_limits<Coordinate>::max()};
    app.view.center_on(edge);
    SDL_Event edge_motion{}; edge_motion.type = SDL_EVENT_MOUSE_MOTION; edge_motion.motion.x = 760; edge_motion.motion.y = 430;
    require(SDL_PushEvent(&edge_motion), "Could not target coordinate boundary"); dispatch(app, renderer);
    key(SDLK_F9); key(SDLK_RIGHT, SDL_KMOD_CTRL); key(SDLK_DOWN); key(SDLK_F8);
    require(inspections.back().starts_with("Cell (2147483647, 2147483647)"), "Keyboard cursor wrapped at coordinate limits");
    key(SDLK_ESCAPE);
    require(app.open(save_path), "Could not restore save workflow fixture");
    require(app.history.apply(app.circuit, std::array{Cell{{44, -17}, Element::source}}).has_value(), "Could not stage save conflict");
    require(save_document(save_path, lost_circuit).has_value(), "Could not stage an external change");
    // The injected test prompt declines replacement; both versions must survive.
    key(SDLK_S, SDL_KMOD_CTRL);
    require(app.history.modified() && load_document(save_path).value() == lost_circuit, "Unapproved conflict overwrote external work");
    app.render(renderer);
    require(SDL_RenderPresent(renderer), "Render failed");
    std::cout << "Desktop smoke passed: SDL events, editing, shared copy/paste, independent New/Open, simulation, rendering\n";
}

struct SdlLifetime { ~SdlLifetime() { SDL_Quit(); } };
struct TestDirectory {
    std::filesystem::path path;
    ~TestDirectory() {
        if (!path.empty()) { std::error_code error; std::filesystem::remove_all(path, error); }
    }
};
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc > 1 ? argv[1] : "";
        if (mode == "--version" && argc == 2) { std::cout << "Gatehaven " << version << " (SDL3)\n"; return 0; }
        if ((mode == "--help" || mode == "-h") && argc == 2) {
            std::cout << "Gatehaven [FILE.ghv|FILE.ccsb | --new | --demo=NAME | --version]\n"
                         "F1: manual  F2: shortcuts  F3: examples  F4: recovery\n";
            return 0;
        }
        const bool child_test = mode == "--self-test-child";
        const bool testing = mode == "--self-test" || child_test;
        const bool snapshot = mode == "--snapshot";
        const bool blank = mode == "--new";
        const bool demo = mode.starts_with("--demo=");
        if (demo && !make_example(mode.substr(7))) {
            std::cerr << "Unknown example. Choose starter, oscillator, screen-switch, positive-relay, negative-relay, or gate-gallery.\n";
            return 2;
        }
        if ((testing && argc != 2) || (snapshot && argc != 3 && argc != 4) || (!testing && !snapshot && argc > 2)) {
            std::cerr << "Usage: gatehaven [FILE.ghv|FILE.ccsb | --new | --demo=NAME | --version | --self-test | --snapshot OUTPUT.bmp [STATE]]\n";
            return 2;
        }
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
        SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
        if (!SDL_SetAppMetadata("Gatehaven", version, application_id) || !SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
        const SdlLifetime lifetime;
        TestDirectory test_directory;
        std::filesystem::path session_directory;
        if (testing || snapshot) {
            test_directory.path = std::filesystem::temp_directory_path() /
                ("gatehaven-smoke-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            session_directory = test_directory.path;
        } else {
            const std::unique_ptr<char, decltype(&SDL_free)> preference(SDL_GetPrefPath("Gatehaven", "Gatehaven"), SDL_free);
            if (!preference) throw std::runtime_error(SDL_GetError());
            session_directory = utf8_path(preference.get()) / "clipboards-v1";
        }
        auto clipboard = ClipboardSession::join(session_directory);
        if (!clipboard) throw std::runtime_error(clipboard.error());
        if (testing || snapshot) SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
        auto flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        if (testing || snapshot) flags |= SDL_WINDOW_HIDDEN;
        SDL_Window* raw_window = nullptr;
        SDL_Renderer* raw_renderer = nullptr;
        const bool created = SDL_CreateWindowAndRenderer("Gatehaven", 1280, 800, flags, &raw_window, &raw_renderer);
        const std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(raw_window, SDL_DestroyWindow);
        const std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(raw_renderer, SDL_DestroyRenderer);
        if (!created) throw std::runtime_error(SDL_GetError());
        if (!SDL_SetRenderLogicalPresentation(renderer.get(), 1280, 800, SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
            throw std::runtime_error(SDL_GetError());
        }
        SDL_SetWindowMinimumSize(window.get(), 800, 500);
        SDL_SetRenderVSync(renderer.get(), 1);
        std::vector<std::optional<std::filesystem::path>> launched;
        std::vector<std::string> demos;
        std::vector<std::string> inspections;
        ui::InstanceLauncher launcher = ui::launch_instance;
        ui::DemoLauncher demo_launcher = ui::launch_demo;
        if (testing) launcher = [&](std::optional<std::filesystem::path> document) -> std::expected<void, std::string> {
            launched.push_back(std::move(document)); return {};
        };
        if (testing) demo_launcher = [&](std::string_view name) -> std::expected<void, std::string> {
            demos.emplace_back(name); return {};
        };
        App app(window.get(), **clipboard, std::move(launcher), std::move(demo_launcher),
                testing ? OverwritePrompt([](SDL_Window*) { return false; }) : OverwritePrompt(confirm_overwrite),
                testing ? InspectionDialog([&](SDL_Window*, const std::string& text) { inspections.push_back(text); return true; }) : InspectionDialog(show_inspection));
        if (!testing && !snapshot) app.load_settings(session_directory.parent_path() / "preferences.ghp");
        if (testing) {
            self_test(app, renderer.get(), session_directory, launched, demos, inspections);
            if (!child_test) {
                const auto child = ui::test_child_process();
                if (!child) throw std::runtime_error(child.error());
            }
            return 0;
        }
        if (snapshot) {
            if (!app.prepare_snapshot(argc == 4 ? argv[3] : "starter")) {
                std::cerr << "Unknown snapshot state. Use a lesson name, help, examples, hints, speed, clipboard, keyboard, or recovery.\n";
                return 2;
            }
            app.simulation.step(app.circuit); app.simulation.step(app.circuit);
            app.render(renderer.get());
            const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> pixels(
                SDL_RenderReadPixels(renderer.get(), nullptr), SDL_DestroySurface);
            if (!pixels || !SDL_SaveBMP(pixels.get(), argv[2])) throw std::runtime_error(SDL_GetError());
            return 0;
        }
        if (blank) app.start_blank();
        else if (demo) app.start_example(mode.substr(7));
        else if (argc == 2 && !app.open(utf8_path(argv[1]))) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Gatehaven", "The circuit could not be opened.", window.get());
        }
        app.enable_recovery(session_directory.parent_path() / "recovery-v1");
        if (argc == 1) app.show_recovery(true);
        auto previous = std::chrono::steady_clock::now();
        while (!app.quit) {
            dispatch(app, renderer.get());
            const auto now = std::chrono::steady_clock::now();
            app.update(std::chrono::duration<double>(now - previous).count());
            previous = now;
            app.render(renderer.get());
            SDL_RenderPresent(renderer.get());
            SDL_Delay(8);
        }
        app.finish_recovery();
        app.save_settings();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Gatehaven: " << e.what() << '\n';
        return 1;
    }
}
