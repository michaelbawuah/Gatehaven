#include "font.hpp"
#include "instances.hpp"
#include "gatehaven/clipboard_session.hpp"
#include "gatehaven/document.hpp"
#include "gatehaven/editor.hpp"
#include "gatehaven/examples.hpp"
#include "gatehaven/simulation.hpp"
#include "gatehaven/selection.hpp"
#include "gatehaven/polyline.hpp"
#include "gatehaven/viewport.hpp"

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
    Element::nor_gate, Element::positive_relay, Element::negative_relay};
constexpr std::array<std::string_view, 10> labels{"WIRE", "CROSSING", "SOURCE", "SIGNAL",
    "AND", "OR", "NAND", "NOR", "+ RELAY", "- RELAY"};

enum class ToolKind { pencil, eraser, panner, selector };
struct InputTool {
    ToolKind kind{ToolKind::pencil};
    Element element{Element::wire};
    bool operator==(const InputTool&) const = default;
};
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

std::filesystem::path utf8_path(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
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

struct DialogResult { bool save{}; bool canceled{}; std::string path; std::string error; };
struct Mailbox { std::mutex mutex; std::optional<DialogResult> result; };
struct DialogRequest { std::shared_ptr<Mailbox> mailbox; bool save; std::string location; };

void SDLCALL dialog_callback(void* userdata, const char* const* paths, int) {
    // Ownership crosses the C API once; the callback reclaims it even on cancel.
    std::unique_ptr<DialogRequest> request(static_cast<DialogRequest*>(userdata));
    DialogResult result;
    result.save = request->save;
    if (!paths) result.error = SDL_GetError();
    else if (!paths[0]) result.canceled = true;
    else result.path = paths[0];
    const std::lock_guard lock(request->mailbox->mutex);
    request->mailbox->result = std::move(result);
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
                 ui::InstanceLauncher launcher = ui::launch_instance)
        : window_(window), clipboards_(clipboards), launcher_(std::move(launcher)) {
        view.area = {240, 96, 1040, 668};
        view.frame(circuit.bounds());
    }

    void start_blank() {
        circuit.clear();
        view.frame(std::nullopt);
        status_ = "NEW CIRCUIT - CHOOSE A COMPONENT TO BEGIN";
    }

    void launch_open(const std::filesystem::path& path) { launch(path); }

    bool open(const std::filesystem::path& path) {
        auto loaded = load_document(path);
        if (!loaded) {
            status_ = "LINE " + std::to_string(loaded.error().line) + ": " + loaded.error().message;
            return false;
        }
        circuit = std::move(*loaded);
        path_ = path;
        history.clear();
        simulation.reset();
        selection_.clear();
        placing_ = false;
        running = false;
        dirty_ = false;
        view.frame(circuit.bounds());
        status_ = "DOCUMENT OPENED";
        return true;
    }

    void update(double elapsed) {
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
                if (result->save) {
                    if (!path.has_extension()) path += ".ghv";
                    save(path);
                } else launch_open(path);
            }
        }
        if (!running || dialog_pending_ || clipboard_menu_ || speed_edit_) { accumulator_ = 0; return; }
        accumulator_ += std::clamp(elapsed, 0.0, 0.25);
        const double interval = 1.0 / speed_;
        unsigned work = 0;
        while (accumulator_ >= interval && work < 8) {
            simulation.step(circuit);
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
        if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            cancel_gesture(); accumulator_ = 0;
        }
        if (e.type == SDL_EVENT_KEY_UP && e.key.key == SDLK_E) eyedropper_ = false;
        if (dialog_pending_) return;
        if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) key(e.key);
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
        }
        if (e.type == SDL_EVENT_MOUSE_MOTION) {
            if (pan_button_) view.pan(e.motion.xrel, e.motion.yrel);
            hover_ = view.area.contains(e.motion.x, e.motion.y) ? view.cell(e.motion.x, e.motion.y) : std::nullopt;
            if (drag_ && hover_) update_preview(*hover_);
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) mouse_down(e.button);
        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && input_button(e.button) == pan_button_) pan_button_.reset();
        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && drag_ && input_button(e.button) == drag_button_) {
            const auto end = view.cell(e.button.x, e.button.y);
            if (end) {
                update_preview(*end);
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
        for (const auto& cell : circuit.cells_in(visible)) {
            draw_cell(r, cell, simulation.ports(cell.position));
            if (selection_.contains(cell.position)) {
                const auto [x, y] = view.screen(cell.position);
                rectangle(r, static_cast<float>(x + 2), static_cast<float>(y + 2),
                    static_cast<float>(view.scale - 4), static_cast<float>(view.scale - 4),
                    selection_changed_ ? SDL_Color{205, 63, 64, 255} : SDL_Color{53, 103, 205, 255}, true);
            }
        }
        for (const auto& cell : preview_) {
            if (visible.contains(cell.position)) draw_cell(r, cell, 0, true);
        }
        if (placing_ && hover_) {
            const auto edits = paste(placement_, *hover_);
            if (edits) for (const auto& cell : *edits) {
                if (visible.contains(cell.position)) draw_cell(r, cell, 0, true);
            }
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
            const float y = 150 + static_cast<float>(i) * 36;
            const InputTool tool{ToolKind::pencil, palette[i]};
            const bool selected = tools_[0] == tool;
            rectangle(r, 12, y, 216, 32, selected ? SDL_Color{225, 241, 236, 255} : white);
            if (selected) rectangle(r, 12, y, 3, 32, teal);
            ui::text(r, 26, y + 10, std::to_string((i + 1) % 10), muted, 1.5F);
            ui::text(r, 52, y + 9, labels[i], selected ? teal : ink, 2.0F);
            draw_bindings(r, y + 9, tool);
        }
        constexpr std::array kinds{ToolKind::selector, ToolKind::panner, ToolKind::eraser};
        constexpr std::array<std::string_view, 3> tool_labels{"Q  SELECT", "PAN", "ERASE"};
        for (std::size_t i = 0; i < kinds.size(); ++i) {
            const float y = 522 + static_cast<float>(i) * 36;
            const InputTool tool{kinds[i]};
            const bool selected = tools_[0] == tool;
            rectangle(r, 12, y, 216, 32, selected ? SDL_Color{225, 241, 236, 255} : paper);
            ui::text(r, 26, y + 10, tool_labels[i], selected ? teal : ink, 1.5F);
            draw_bindings(r, y + 9, tool);
        }
        ui::text(r, 22, 645, "CLICK A TOOL TO BIND", ink, 1.25F);
        for (std::size_t i = 0; i < binding_names.size(); ++i) {
            ui::text(r, 22 + static_cast<float>(i % 3) * 68, 667 + static_cast<float>(i / 3) * 15,
                     binding_names[i], binding_colors[i], 1);
        }
        ui::text(r, 22, 707, "SHARED CLIPBOARD " + std::to_string(clipboard_), ink, 1.25F);
        ui::text(r, 22, 729, "CTRL SHIFT C/V: CHOOSE", muted, 1.0F);
        ui::text(r, 22, 748, "B: KEYBOARD HELP", muted, 1.0F);
        rectangle(r, 0, 764, 1280, 36, ink);
        ui::text(r, 20, 771, status_.substr(0, 73), white, 1.25F);
        if (hover_) {
            const auto info = std::string(name(circuit.at(*hover_))) + "  [" + std::to_string(hover_->x) +
                ", " + std::to_string(hover_->y) + "]  " + (simulation.powered(*hover_) ? "ON" : "OFF");
            ui::text(r, 20, 786, info, {170, 208, 196, 255}, 1);
        }
        ui::text(r, 786, 777, (dirty_ ? "*  " : "") + std::to_string(circuit.size()) + " CELLS    TICK " +
                 std::to_string(simulation.ticks()) + (running ? "    RUNNING" : "    PAUSED"), white, 1.25F);
        if (help_) render_help(r);
        if (clipboard_menu_) render_clipboard_menu(r);
        if (speed_edit_) render_speed_dialog(r);
        if (dialog_pending_) {
            rectangle(r, 414, 334, 548, 92, ink);
            ui::text(r, 440, 373, "CHOOSE A FILE IN THE SYSTEM DIALOG", white, 2);
        }
    }

private:
    SDL_Window* window_; // Non-owning; main owns the window for the entire App lifetime.
    std::array<InputTool, 6> tools_{{{ToolKind::pencil}, {ToolKind::eraser}, {ToolKind::panner},
        {ToolKind::selector}, {ToolKind::panner}, {ToolKind::pencil}}};
    InputTool drag_tool_;
    bool eyedropper_{};
    bool placing_{};
    bool dirty_{};
    bool help_{};
    bool dialog_pending_{};
    std::optional<Point> hover_;
    std::optional<Point> drag_;
    std::size_t drag_button_{};
    std::optional<std::size_t> pan_button_;
    std::vector<Cell> preview_;
    Selection selection_;
    SelectionMode selection_mode_{SelectionMode::replace};
    bool selection_changed_{};
    ClipboardSession& clipboards_;
    ui::InstanceLauncher launcher_;
    Stamp placement_;
    unsigned clipboard_{};
    std::optional<char> clipboard_menu_;
    std::filesystem::path path_;
    std::string status_{"WELCOME - EXPLORE THE STARTER CIRCUIT"};
    std::shared_ptr<Mailbox> mailbox_{std::make_shared<Mailbox>()};
    unsigned speed_{5};
    std::optional<std::string> speed_edit_;
    bool speed_replace_{};
    bool speed_error_{};
    double accumulator_{};

    void cancel_gesture() {
        drag_.reset();
        pan_button_.reset();
        preview_.clear();
        eyedropper_ = false;
    }

    bool apply(std::span<const Cell> edits) {
        const auto result = history.apply(circuit, edits);
        if (!result) { status_ = result.error(); return false; }
        if (*result) {
            dirty_ = true;
            simulation.invalidate(history.last_changes());
            status_ = "CIRCUIT UPDATED";
        }
        return true;
    }

    void update_preview(Point end) {
        preview_.clear();
        if (!drag_ || drag_tool_.kind == ToolKind::selector) return;
        auto element = drag_tool_.kind == ToolKind::eraser ? Element::empty : drag_tool_.element;
        if (*drag_ == end && circuit.at(end) == element && element >= Element::positive_relay) {
            element = Element::signal;
        }
        const auto stroke = pencil_line(*drag_, end, element);
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
                case 1: running = false; simulation.step(circuit); break;
                case 2: simulation.reset(); accumulator_ = 0; break;
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
        if (ViewRect{12, 150, 216, 360}.contains(e.x, e.y)) {
            tools_[*button] = {ToolKind::pencil, palette[static_cast<std::size_t>((e.y - 150) / 36)]};
            placing_ = false; return;
        }
        if (ViewRect{12, 522, 216, 108}.contains(e.x, e.y)) {
            constexpr std::array kinds{ToolKind::selector, ToolKind::panner, ToolKind::eraser};
            tools_[*button] = {kinds[static_cast<std::size_t>((e.y - 522) / 36)]};
            placing_ = false; return;
        }
        if (!view.area.contains(e.x, e.y)) return;
        hover_ = view.cell(e.x, e.y);
        if (!hover_) return;
        if (eyedropper_) {
            const auto element = circuit.at(*hover_);
            tools_[*button] = element == Element::empty ? InputTool{ToolKind::eraser} : InputTool{ToolKind::pencil, element};
            status_ = "BOUND " + std::string(binding_names[*button]) + " TO " + std::string(name(element));
            placing_ = false; return;
        }
        if (placing_ && e.button == SDL_BUTTON_LEFT) {
            const auto edits = paste(placement_, *hover_);
            if (edits) apply(*edits); else status_ = edits.error();
            placing_ = false; return;
        }
        if (drag_ || pan_button_) return; // One gesture at a time; release its owning button to finish.
        if (tools_[*button].kind == ToolKind::panner) { pan_button_ = *button; return; }
        const auto modifiers = SDL_GetModState();
        selection_mode_ = (modifiers & SDL_KMOD_ALT) != 0 ? SelectionMode::subtract :
            (modifiers & SDL_KMOD_SHIFT) != 0 ? SelectionMode::add : SelectionMode::replace;
        if (tools_[*button].kind == ToolKind::selector && e.clicks >= 2) {
            selection_.combine(connected_selection(circuit, *hover_, e.clicks >= 3), selection_mode_);
            selection_changed_ = false; return;
        }
        drag_ = hover_;
        drag_button_ = *button;
        drag_tool_ = tools_[*button];
        update_preview(*drag_);
    }

    void key(const SDL_KeyboardEvent& e) {
        const bool control = (e.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
        const bool shift = (e.mod & SDL_KMOD_SHIFT) != 0;
        if (speed_edit_) { speed_key(e.key); return; }
        if (clipboard_menu_) {
            if (e.key == SDLK_ESCAPE) clipboard_menu_.reset();
            else if (e.key >= SDLK_0 && e.key <= SDLK_9) choose_clipboard(static_cast<unsigned>(e.key - SDLK_0));
            else if (e.key == SDLK_LEFT) clipboard_ = (clipboard_ + 9) % 10;
            else if (e.key == SDLK_RIGHT) clipboard_ = (clipboard_ + 1) % 10;
            else if (e.key == SDLK_RETURN) choose_clipboard(clipboard_);
            return;
        }
        if (e.key == SDLK_ESCAPE) {
            cancel_gesture(); selection_.clear(); placing_ = false; help_ = false; return;
        }
        if (selection_ && !placing_ && (e.key == SDLK_LEFT || e.key == SDLK_RIGHT || e.key == SDLK_UP || e.key == SDLK_DOWN)) {
            const std::int64_t distance = control ? 4 : 1;
            const auto dx = e.key == SDLK_LEFT ? -distance : e.key == SDLK_RIGHT ? distance : 0;
            const auto dy = e.key == SDLK_UP ? -distance : e.key == SDLK_DOWN ? distance : 0;
            const auto moved = move_selection(circuit, selection_, dx, dy);
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
            case SDLK_SPACE: edit_speed(); break;
            default: break;
            }
            return;
        }
        if (e.key >= SDLK_0 && e.key <= SDLK_9) {
            const auto digit = static_cast<std::size_t>(e.key - SDLK_0);
            tools_[0] = {ToolKind::pencil, palette[(digit + 9) % 10]}; placing_ = false; return;
        }
        switch (e.key) {
        case SDLK_SPACE: running = !running; accumulator_ = 0; break;
        case SDLK_RIGHT: running = false; simulation.step(circuit); break;
        case SDLK_R: simulation.reset(); accumulator_ = 0; break;
        case SDLK_Q: tools_[0] = {ToolKind::selector}; placing_ = false; break;
        case SDLK_E: eyedropper_ = true; break;
        case SDLK_B: cancel_gesture(); help_ = !help_; break;
        case SDLK_F: view.frame(circuit.bounds()); break;
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
        if (redo ? history.redo(circuit) : history.undo(circuit)) {
            dirty_ = true; simulation.invalidate(history.last_changes());
            status_ = redo ? "REDONE" : "UNDONE";
        }
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
        const auto result = clipboards_.write(clipboard_, capture_selection(circuit, selection_));
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
        placing_ = !placement_.cells.empty();
        status_ = placing_ ? "CLICK TO PLACE - BRACKETS ROTATE" : "CLIPBOARD IS EMPTY";
    }

    void erase_selection() {
        if (!selection_) return;
        auto edits = selection_.cells(circuit);
        for (auto& cell : edits) cell.element = Element::empty;
        if (apply(edits)) selection_.clear();
    }

    void transform(char operation) {
        if (!placing_ && !selection_) return;
        auto stamp = placing_ ? placement_ : capture_selection(circuit, selection_);
        if (operation == 'h') stamp.flip_horizontal();
        else if (operation == 'v') stamp.flip_vertical();
        else {
            const auto rotations = operation == 'l' ? 3 : 1;
            for (int i = 0; i < rotations; ++i) stamp.rotate_clockwise();
        }
        if (placing_) { placement_ = std::move(stamp); return; }
        const auto result = place_selection(circuit, selection_, stamp, selection_.bounds()->min);
        if (!result) { status_ = result.error(); return; }
        if (apply(result->edits)) { selection_ = result->selection; selection_changed_ = true; }
    }

    bool discard_changes() {
        if (!dirty_) return true;
        running = false;
        const SDL_MessageBoxButtonData choices[]{
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"},
            {0, 1, "Discard changes"}};
        const SDL_MessageBoxData data{SDL_MESSAGEBOX_WARNING, window_, "Unsaved circuit",
            "This circuit has unsaved changes. Cancel to save it first, or discard the changes.",
            2, choices, nullptr};
        int selected = 0;
        return SDL_ShowMessageBox(&data, &selected) && selected == 1;
    }

    void launch(std::optional<std::filesystem::path> document) {
        const auto result = launcher_(std::move(document));
        status_ = result ? "OPENED IN A NEW GATEHAVEN WINDOW" : "NEW WINDOW FAILED: " + result.error();
    }

    void fresh() { launch(std::nullopt); }

    void save(const std::filesystem::path& path) {
        const auto result = save_document(path, circuit);
        if (!result) { status_ = result.error().message; return; }
        path_ = path; dirty_ = false; status_ = "CIRCUIT SAVED";
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
        static const SDL_DialogFileFilter filter{"Gatehaven circuit", "ghv"};
        const auto utf8 = path_.empty() ? std::u8string(u8"circuit.ghv") : path_.u8string();
        auto request = std::make_unique<DialogRequest>(DialogRequest{mailbox_, saving, {utf8.begin(), utf8.end()}});
        const auto location = request->location.c_str();
        if (saving) SDL_ShowSaveFileDialog(dialog_callback, request.release(), window_, &filter, 1, location);
        else SDL_ShowOpenFileDialog(dialog_callback, request.release(), window_, &filter, 1, nullptr, false);
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
            const auto label = cell.element == Element::source ? std::string_view("+") :
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

    void render_help(SDL_Renderer* r) const {
        rectangle(r, 338, 132, 846, 608, ink);
        ui::text(r, 376, 194, "BUILD YOUR FIRST CIRCUIT", white, 2.5F);
        constexpr std::array<std::string_view, 13> lines{
            "1-0: COMPONENTS      Q: SELECT REGION",
            "CLICK TOOL: BIND THAT MOUSE BUTTON",
            "HOLD E + CLICK: SAMPLE A TOOL",
            "SPACE: PLAY/PAUSE    RIGHT: ONE TICK",
            "CTRL SPACE: SET TICKS PER SECOND",
            "R: RESET            F: FRAME CIRCUIT",
            "CTRL C/X/V: COPY / CUT / PASTE",
            "CTRL Z/Y: UNDO / REDO",
            "CTRL S/O/N: SAVE / OPEN / NEW",
            "[ AND ]: ROTATE     H/V: FLIP",
            "ARROWS: MOVE SELECTION  CTRL: X4",
            "CTRL SHIFT C/V: CHOOSE CLIPBOARD",
            "B OR ESC: CLOSE THIS HELP"};
        for (std::size_t i = 0; i < lines.size(); ++i) {
            ui::text(r, 378, 246 + static_cast<float>(i) * 36, lines[i], white, 1.75F);
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
               const std::vector<std::optional<std::filesystem::path>>& launched) {
    const auto require = [](bool ok, const char* message) {
        if (!ok) throw std::runtime_error(message);
    };
    const auto key = [&](SDL_Keycode code, SDL_Keymod mod = SDL_KMOD_NONE) {
        SDL_Event event{}; event.type = SDL_EVENT_KEY_DOWN;
        event.key.key = code; event.key.mod = mod;
        require(SDL_PushEvent(&event), "Could not push keyboard event");
        dispatch(app, renderer);
    };
    const auto original = app.circuit;
    key(SDLK_SPACE);
    require(app.running, "Play event failed");
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
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, {-1, -4});
    require(app.circuit.size() == original.size() + 5, "Mouse stroke failed");
    key(SDLK_Z, SDL_KMOD_CTRL);
    require(app.circuit == original, "Keyboard undo failed");
    key(SDLK_Y, SDL_KMOD_CTRL);
    require(app.circuit.size() == original.size() + 5, "Keyboard redo failed");
    key(SDLK_R);
    require(app.simulation.ticks() == 0, "Reset failed");
    key(SDLK_SPACE, SDL_KMOD_CTRL);
    key(SDLK_2); key(SDLK_3); key(SDLK_RETURN);
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
    constexpr std::array<Uint8, 5> inputs{SDL_BUTTON_LEFT, SDL_BUTTON_RIGHT, SDL_BUTTON_MIDDLE, SDL_BUTTON_X1, SDL_BUTTON_X2};
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, 94, 238, inputs[i], 0); // Bind this button to Source.
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, 94, 238, inputs[i], 0);
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
        const bool child_test = mode == "--self-test-child";
        const bool testing = mode == "--self-test" || child_test;
        const bool snapshot = mode == "--snapshot";
        const bool blank = mode == "--new";
        if ((testing && argc != 2) || (snapshot && argc != 3) || (!testing && !snapshot && argc > 2)) {
            std::cerr << "Usage: gatehaven [FILE.ghv | --new | --self-test | --snapshot OUTPUT.bmp]\n";
            return 2;
        }
        if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
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
        ui::InstanceLauncher launcher = ui::launch_instance;
        if (testing) launcher = [&](std::optional<std::filesystem::path> document) -> std::expected<void, std::string> {
            launched.push_back(std::move(document)); return {};
        };
        App app(window.get(), **clipboard, std::move(launcher));
        if (testing) {
            self_test(app, renderer.get(), session_directory, launched);
            if (!child_test) {
                const auto child = ui::test_child_process();
                if (!child) throw std::runtime_error(child.error());
            }
            return 0;
        }
        if (snapshot) {
            app.simulation.step(app.circuit); app.simulation.step(app.circuit);
            app.render(renderer.get());
            const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> pixels(
                SDL_RenderReadPixels(renderer.get(), nullptr), SDL_DestroySurface);
            if (!pixels || !SDL_SaveBMP(pixels.get(), argv[2])) throw std::runtime_error(SDL_GetError());
            return 0;
        }
        if (blank) app.start_blank();
        else if (argc == 2 && !app.open(utf8_path(argv[1]))) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Gatehaven", "The circuit could not be opened.", window.get());
        }
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
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Gatehaven: " << e.what() << '\n';
        return 1;
    }
}
