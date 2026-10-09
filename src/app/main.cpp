#include "font.hpp"
#include "instances.hpp"
#include "gatehaven/clipboard_session.hpp"
#include "gatehaven/document.hpp"
#include "gatehaven/editor.hpp"
#include "gatehaven/examples.hpp"
#include "gatehaven/simulation.hpp"
#include "gatehaven/viewport.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <array>
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
        selection_.reset();
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
        if (!running || dialog_pending_ || clipboard_menu_) { accumulator_ = 0; return; }
        accumulator_ += std::clamp(elapsed, 0.0, 0.25);
        const double interval = 1.0 / speeds_[speed_index_];
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
        if (dialog_pending_) return;
        if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            drag_.reset(); preview_.clear(); accumulator_ = 0;
        }
        if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) key(e.key);
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
            hover_ = view.area.contains(e.motion.x, e.motion.y) ? view.cell(e.motion.x, e.motion.y) : std::nullopt;
            if ((e.motion.state & SDL_BUTTON_MMASK) != 0) view.pan(e.motion.xrel, e.motion.yrel);
            if (drag_ && hover_) update_preview(*hover_);
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) mouse_down(e.button);
        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && drag_ && e.button.button == drag_button_) {
            const auto end = view.cell(e.button.x, e.button.y);
            if (end) {
                update_preview(*end);
                if (selecting_ && drag_button_ == SDL_BUTTON_LEFT) {
                    selection_ = Bounds{{std::min(drag_->x, end->x), std::min(drag_->y, end->y)},
                                        {std::max(drag_->x, end->x), std::max(drag_->y, end->y)}};
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
        for (const auto& cell : circuit.cells_in(visible)) draw_cell(r, cell, simulation.ports(cell.position));
        for (const auto& cell : preview_) {
            if (visible.contains(cell.position)) draw_cell(r, cell, 0, true);
        }
        if (placing_ && hover_) {
            const auto edits = paste(placement_, *hover_);
            if (edits) for (const auto& cell : *edits) {
                if (visible.contains(cell.position)) draw_cell(r, cell, 0, true);
            }
        }
        if (selection_) draw_selection(r, *selection_);
        if (selecting_ && drag_ && hover_) {
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
        const auto toolbar = buttons(running, speeds_[speed_index_]);
        for (std::size_t i = 0; i < toolbar.size(); ++i) {
            const auto& button = toolbar[i];
            rectangle(r, static_cast<float>(button.rect.x), static_cast<float>(button.rect.y),
                      static_cast<float>(button.rect.width), static_cast<float>(button.rect.height),
                      i == 0 ? teal : paper);
            const auto text_width = static_cast<float>(button.label.size()) * 9;
            ui::text(r, static_cast<float>(button.rect.x + button.rect.width / 2) - text_width / 2,
                     37, button.label, i == 0 ? white : ink, 1.5F);
        }
        ui::text(r, 276, 76, "SPACE: PLAY / PAUSE    RIGHT ARROW: STEP    MIDDLE DRAG: PAN    SCROLL: ZOOM", muted, 1.25F);
        ui::text(r, 22, 121, "COMPONENTS", muted, 1.5F);
        for (std::size_t i = 0; i < palette.size(); ++i) {
            const float y = 152 + static_cast<float>(i) * 40;
            const bool selected = !selecting_ && palette[i] == tool_;
            rectangle(r, 12, y, 216, 34, selected ? SDL_Color{225, 241, 236, 255} : white);
            if (selected) rectangle(r, 12, y, 3, 34, teal);
            ui::text(r, 26, y + 10, std::to_string((i + 1) % 10), muted, 1.5F);
            ui::text(r, 52, y + 9, labels[i], selected ? teal : ink, 2.0F);
        }
        rectangle(r, 12, 566, 216, 34, selecting_ ? SDL_Color{225, 241, 236, 255} : paper);
        ui::text(r, 26, 576, "Q  SELECT REGION", selecting_ ? teal : ink, 1.5F);
        line(r, 22, 619, 216, 619, border);
        ui::text(r, 22, 641, "LEFT DRAG TO DRAW", ink, 1.25F);
        ui::text(r, 22, 664, "RIGHT DRAG TO ERASE", muted, 1.25F);
        ui::text(r, 22, 693, "SHARED CLIPBOARD " + std::to_string(clipboard_), ink, 1.25F);
        ui::text(r, 22, 716, "CTRL SHIFT C/V: CHOOSE", muted, 1.0F);
        ui::text(r, 22, 738, "B: KEYBOARD HELP", muted, 1.0F);
        rectangle(r, 0, 764, 1280, 36, ink);
        std::string status = status_;
        if (hover_) status = std::string(name(circuit.at(*hover_))) + "  [" + std::to_string(hover_->x) +
                            ", " + std::to_string(hover_->y) + "]  " + (simulation.powered(*hover_) ? "ON" : "OFF");
        ui::text(r, 20, 777, status.substr(0, 73), white, 1.25F);
        ui::text(r, 786, 777, (dirty_ ? "*  " : "") + std::to_string(circuit.size()) + " CELLS    TICK " +
                 std::to_string(simulation.ticks()) + (running ? "    RUNNING" : "    PAUSED"), white, 1.25F);
        if (help_) render_help(r);
        if (clipboard_menu_) render_clipboard_menu(r);
        if (dialog_pending_) {
            rectangle(r, 414, 334, 548, 92, ink);
            ui::text(r, 440, 373, "CHOOSE A FILE IN THE SYSTEM DIALOG", white, 2);
        }
    }

private:
    SDL_Window* window_; // Non-owning; main owns the window for the entire App lifetime.
    Element tool_{Element::wire};
    bool selecting_{};
    bool placing_{};
    bool dirty_{};
    bool help_{};
    bool dialog_pending_{};
    std::optional<Point> hover_;
    std::optional<Point> drag_;
    std::uint8_t drag_button_{};
    std::vector<Cell> preview_;
    std::optional<Bounds> selection_;
    ClipboardSession& clipboards_;
    ui::InstanceLauncher launcher_;
    Stamp placement_;
    unsigned clipboard_{};
    std::optional<char> clipboard_menu_;
    std::filesystem::path path_;
    std::string status_{"WELCOME - EXPLORE THE STARTER CIRCUIT"};
    std::shared_ptr<Mailbox> mailbox_{std::make_shared<Mailbox>()};
    static constexpr std::array<unsigned, 5> speeds_{1, 5, 15, 30, 60};
    std::size_t speed_index_{1};
    double accumulator_{};

    void apply(std::span<const Cell> edits) {
        const auto result = history.apply(circuit, edits);
        if (!result) { status_ = result.error(); return; }
        if (*result) {
            dirty_ = true;
            simulation.invalidate(history.last_changes());
            status_ = "CIRCUIT UPDATED";
        }
    }

    void update_preview(Point end) {
        preview_.clear();
        if (!drag_ || (selecting_ && drag_button_ == SDL_BUTTON_LEFT)) return;
        auto element = drag_button_ == SDL_BUTTON_RIGHT ? Element::empty : tool_;
        if (*drag_ == end && circuit.at(end) == element && element >= Element::positive_relay) {
            element = Element::signal;
        }
        const auto stroke = pencil_line(*drag_, end, element);
        if (stroke) preview_ = *stroke; else status_ = stroke.error();
    }

    void mouse_down(const SDL_MouseButtonEvent& e) {
        if (e.button != SDL_BUTTON_LEFT && e.button != SDL_BUTTON_RIGHT) return;
        if (help_) { help_ = false; return; }
        if (e.button == SDL_BUTTON_LEFT) {
            const auto toolbar = buttons(running, speeds_[speed_index_]);
            for (std::size_t i = 0; i < toolbar.size(); ++i) {
                if (!toolbar[i].rect.contains(e.x, e.y)) continue;
                switch (i) {
                case 0: running = !running; accumulator_ = 0; break;
                case 1: running = false; simulation.step(circuit); break;
                case 2: simulation.reset(); accumulator_ = 0; break;
                case 3: undo(false); break;
                case 4: undo(true); break;
                case 5: file_dialog(false); break;
                case 6: request_save(false); break;
                case 7: view.frame(circuit.bounds()); break;
                case 8: speed_index_ = (speed_index_ + 1) % speeds_.size(); accumulator_ = 0; break;
                case 9: fresh(); break;
                default: break;
                }
                return;
            }
            if (e.x < 240 && e.y >= 152 && e.y < 552) {
                tool_ = palette[static_cast<std::size_t>((e.y - 152) / 40)];
                selecting_ = false; placing_ = false; return;
            }
            if (ViewRect{12, 566, 216, 34}.contains(e.x, e.y)) {
                selecting_ = true; placing_ = false; return;
            }
        }
        if (!view.area.contains(e.x, e.y)) return;
        hover_ = view.cell(e.x, e.y);
        if (!hover_) return;
        if (placing_ && e.button == SDL_BUTTON_LEFT) {
            const auto edits = paste(placement_, *hover_);
            if (edits) apply(*edits); else status_ = edits.error();
            placing_ = false; return;
        }
        drag_ = hover_;
        drag_button_ = e.button;
        update_preview(*drag_);
    }

    void key(const SDL_KeyboardEvent& e) {
        const bool control = (e.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
        const bool shift = (e.mod & SDL_KMOD_SHIFT) != 0;
        if (clipboard_menu_) {
            if (e.key == SDLK_ESCAPE) clipboard_menu_.reset();
            else if (e.key >= SDLK_0 && e.key <= SDLK_9) choose_clipboard(static_cast<unsigned>(e.key - SDLK_0));
            else if (e.key == SDLK_LEFT) clipboard_ = (clipboard_ + 9) % 10;
            else if (e.key == SDLK_RIGHT) clipboard_ = (clipboard_ + 1) % 10;
            else if (e.key == SDLK_RETURN) choose_clipboard(clipboard_);
            return;
        }
        if (e.key == SDLK_ESCAPE) {
            drag_.reset(); preview_.clear(); selection_.reset(); placing_ = false; help_ = false; return;
        }
        if (control) {
            switch (e.key) {
            case SDLK_S: request_save(shift); break;
            case SDLK_O: file_dialog(false); break;
            case SDLK_N: fresh(); break;
            case SDLK_Z: undo(shift); break;
            case SDLK_Y: undo(true); break;
            case SDLK_A: selection_ = circuit.bounds(); selecting_ = true; break;
            case SDLK_C: clipboard_action('c', shift); break;
            case SDLK_X: clipboard_action('x', shift); break;
            case SDLK_V: clipboard_action('v', shift); break;
            case SDLK_SPACE: speed_index_ = (speed_index_ + 1) % speeds_.size(); accumulator_ = 0; break;
            default: break;
            }
            return;
        }
        if (e.key >= SDLK_0 && e.key <= SDLK_9) {
            const auto digit = static_cast<std::size_t>(e.key - SDLK_0);
            tool_ = palette[(digit + 9) % 10]; selecting_ = false; placing_ = false; return;
        }
        switch (e.key) {
        case SDLK_SPACE: running = !running; accumulator_ = 0; break;
        case SDLK_RIGHT: running = false; simulation.step(circuit); break;
        case SDLK_R: simulation.reset(); accumulator_ = 0; break;
        case SDLK_Q: selecting_ = true; placing_ = false; break;
        case SDLK_B: help_ = !help_; break;
        case SDLK_F: view.frame(circuit.bounds()); break;
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

    void copy(bool cut) {
        if (!selection_) { status_ = "SELECT A REGION FIRST"; return; }
        const auto result = clipboards_.write(clipboard_, capture(circuit, *selection_));
        if (!result) { status_ = "COPY FAILED: " + result.error(); return; }
        if (cut) erase_selection();
        status_ = "COPIED TO SHARED CLIPBOARD " + std::to_string(clipboard_);
    }

    void clipboard_action(char action, bool choose) {
        if (action != 'v' && !selection_) { status_ = "SELECT A REGION FIRST"; return; }
        drag_.reset(); preview_.clear(); placing_ = false;
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
        selecting_ = false;
        status_ = placing_ ? "CLICK TO PLACE - BRACKETS ROTATE" : "CLIPBOARD IS EMPTY";
    }

    void erase_selection() {
        if (!selection_) return;
        auto edits = circuit.cells_in(*selection_);
        for (auto& cell : edits) cell.element = Element::empty;
        apply(edits);
    }

    void transform(char operation) {
        if (!placing_ && !selection_) return;
        auto stamp = placing_ ? placement_ : capture(circuit, *selection_);
        if (operation == 'h') stamp.flip_horizontal();
        else if (operation == 'v') stamp.flip_vertical();
        else {
            const auto rotations = operation == 'l' ? 3 : 1;
            for (int i = 0; i < rotations; ++i) stamp.rotate_clockwise();
        }
        if (placing_) { placement_ = std::move(stamp); return; }
        const auto target = paste(stamp, selection_->min);
        const auto corner = translated(selection_->min, stamp.width - 1, stamp.height - 1);
        if (!target || !corner) { status_ = "TRANSFORM EXCEEDS WORLD BOUNDARY"; return; }
        auto edits = circuit.cells_in(*selection_);
        for (auto& cell : edits) cell.element = Element::empty;
        edits.insert(edits.end(), target->begin(), target->end());
        apply(edits);
        selection_->max = *corner;
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
        running = false;
        dialog_pending_ = true;
        static const SDL_DialogFileFilter filter{"Gatehaven circuit", "ghv"};
        const auto utf8 = path_.empty() ? std::u8string(u8"circuit.ghv") : path_.u8string();
        auto request = std::make_unique<DialogRequest>(DialogRequest{mailbox_, saving, {utf8.begin(), utf8.end()}});
        const auto location = request->location.c_str();
        if (saving) SDL_ShowSaveFileDialog(dialog_callback, request.release(), window_, &filter, 1, location);
        else SDL_ShowOpenFileDialog(dialog_callback, request.release(), window_, &filter, 1, nullptr, false);
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
        rectangle(r, 338, 158, 846, 556, ink);
        ui::text(r, 376, 194, "BUILD YOUR FIRST CIRCUIT", white, 2.5F);
        constexpr std::array<std::string_view, 11> lines{
            "1-0: COMPONENTS      Q: SELECT REGION",
            "LEFT DRAG: DRAW      RIGHT DRAG: ERASE",
            "MIDDLE DRAG: PAN     SCROLL: ZOOM",
            "SPACE: PLAY/PAUSE    RIGHT: ONE TICK",
            "R: RESET            F: FRAME CIRCUIT",
            "CTRL C/X/V: COPY / CUT / PASTE",
            "CTRL Z/Y: UNDO / REDO",
            "CTRL S/O/N: SAVE / OPEN / NEW",
            "[ AND ]: ROTATE     H/V: FLIP",
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
    const auto mouse = [&](Uint32 type, Point cell) {
        const auto [x, y] = app.view.screen(cell);
        SDL_Event event{}; event.type = type; event.button.button = SDL_BUTTON_LEFT;
        event.button.x = static_cast<float>(x + app.view.scale / 2);
        event.button.y = static_cast<float>(y + app.view.scale / 2);
        require(SDL_PushEvent(&event), "Could not push mouse event");
        dispatch(app, renderer);
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
