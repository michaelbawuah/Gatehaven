#pragma once
#include "gatehaven/circuit.hpp"
#include "gatehaven/connectivity.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace gatehaven::ui {
inline bool is_gate(Element element) {
    return element >= Element::and_gate && element <= Element::nor_gate;
}

// Orientation is presentation only. Signal cells can control a gate from any
// side; prefer an actual conductive neighbor for the pointed output side.
inline Direction symbol_direction(const Circuit& circuit, Point position, Element element) {
    constexpr std::array order{Direction::east, Direction::north, Direction::west, Direction::south};
    for (const auto direction : order) {
        const auto next = neighbor(position, direction);
        if (next && conducts_between(element, circuit.at(*next))) return direction;
    }
    for (const auto direction : order) {
        const auto next = neighbor(position, direction);
        if (next && circuit.at(*next) == Element::signal)
            return static_cast<Direction>((static_cast<unsigned>(direction) + 2U) % 4U);
    }
    return Direction::east;
}

// Batch vector strokes into one geometry submission per symbol. No font, bitmap
// scaling or per-segment allocation is involved, even at the closest zoom.
class SymbolStrokes {
public:
    SymbolStrokes(SDL_Renderer* renderer, float x, float y, float size, SDL_Color color, Direction direction)
        : renderer_(renderer), x_(x), y_(y), scale_(size / 24),
          width_(std::clamp(size * .055F, .85F, 3.5F) / scale_),
          color_{color.r / 255.0F, color.g / 255.0F, color.b / 255.0F, color.a / 255.0F},
          turns_((static_cast<unsigned>(direction) + 3U) % 4U), segments_(size < 16 ? 6 : size < 48 ? 12 : 24) {}

    void line(float x1, float y1, float x2, float y2) {
        const float dx = x2 - x1, dy = y2 - y1, length = std::hypot(dx, dy);
        if (length == 0) return;
        if (count_ + 4 > static_cast<int>(vertices_.size())) draw();
        const float ox = -dy / length * width_ / 2, oy = dx / length * width_ / 2;
        // Overlap neighboring segments so curve joints stay closed at 1x scale.
        const float ex = dx / length * width_ * .4F, ey = dy / length * width_ * .4F;
        const std::array<SDL_FPoint, 4> corners{{{x1 - ex + ox, y1 - ey + oy}, {x2 + ex + ox, y2 + ey + oy},
                                              {x2 + ex - ox, y2 + ey - oy}, {x1 - ex - ox, y1 - ey - oy}}};
        for (const auto point : corners) vertices_[static_cast<std::size_t>(count_++)] = {map(point), color_, {}};
        const int start = count_ - 4;
        for (const int offset : {0, 1, 2, 0, 2, 3}) indices_[static_cast<std::size_t>(index_count_++)] = start + offset;
    }
    void curve(SDL_FPoint from, SDL_FPoint control, SDL_FPoint to) {
        auto previous = from;
        for (int i = 1; i <= segments_; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(segments_), u = 1 - t;
            const SDL_FPoint point{u * u * from.x + 2 * u * t * control.x + t * t * to.x,
                                   u * u * from.y + 2 * u * t * control.y + t * t * to.y};
            line(previous.x, previous.y, point.x, point.y); previous = point;
        }
    }
    void arc(float cx, float cy, float rx, float ry, float from, float to) {
        for (int i = 0; i < segments_; ++i) {
            const float a = from + (to - from) * static_cast<float>(i) / static_cast<float>(segments_);
            const float b = from + (to - from) * static_cast<float>(i + 1) / static_cast<float>(segments_);
            line(cx + rx * std::cos(a), cy + ry * std::sin(a), cx + rx * std::cos(b), cy + ry * std::sin(b));
        }
    }
    void circle(float x, float y, float radius) { arc(x, y, radius, radius, 0, 2 * std::numbers::pi_v<float>); }
    void box(float x, float y, float w, float h) {
        line(x, y, x + w, y); line(x + w, y, x + w, y + h); line(x + w, y + h, x, y + h); line(x, y + h, x, y);
    }
    void draw() {
        if (count_) SDL_RenderGeometry(renderer_, nullptr, vertices_.data(), count_, indices_.data(), index_count_);
        count_ = index_count_ = 0;
    }
private:
    SDL_FPoint map(SDL_FPoint point) const {
        for (unsigned i = 0; i < turns_; ++i) point = {24 - point.y, point.x};
        return {x_ + point.x * scale_, y_ + point.y * scale_};
    }
    SDL_Renderer* renderer_;
    float x_, y_, scale_, width_;
    SDL_FColor color_;
    unsigned turns_;
    int segments_;
    std::array<SDL_Vertex, 512> vertices_;
    std::array<int, 768> indices_;
    int count_{}, index_count_{};
};

inline void component_symbol(SDL_Renderer* renderer, Element element, float x, float y, float size,
                             SDL_Color color, Direction direction = Direction::east, bool conducting = false, bool leads = false) {
    SymbolStrokes p(renderer, x, y, size, color, direction);
    const float pi = std::numbers::pi_v<float>;
    if (is_gate(element)) {
        const bool inverted = element == Element::nand_gate || element == Element::nor_gate;
        const bool disjunction = element == Element::or_gate || element == Element::nor_gate;
        const float nose = inverted ? 18.0F : 21.0F;
        if (disjunction) {
            p.curve({4, 4}, {15, 4}, {nose, 12});
            p.curve({nose, 12}, {15, 20}, {4, 20});
            p.curve({4, 20}, {10, 12}, {4, 4});
        } else {
            p.line(5, 4, 12, 4); p.line(5, 4, 5, 20); p.line(5, 20, 12, 20);
            p.arc(12, 12, nose - 12, 8, -pi / 2, pi / 2);
        }
        if (inverted) p.circle(20, 12, 2); // Hollow inversion bubble.
        if (leads) {
            p.line(0, 8, disjunction ? 5.5F : 5.0F, 8); p.line(0, 16, disjunction ? 5.5F : 5.0F, 16);
            p.line(inverted ? 22.0F : nose, 12, 24, 12);
        }
    } else if (element == Element::source) {
        p.circle(12, 12, 8); p.line(8, 12, 16, 12); p.line(12, 8, 12, 16);
    } else if (is_relay(element)) {
        p.line(2, 16, 7, 16); p.circle(8, 16, 1); p.circle(17, 16, 1); p.line(18, 16, 22, 16);
        p.line(8, 16, 17, conducting ? 16.0F : 10.0F);
        p.line(5, 5, 11, 5); if (element == Element::positive_relay) p.line(8, 2, 8, 8);
    } else if (element == Element::screen) {
        p.box(3, 4, 18, 14); p.line(8, 21, 16, 21); p.line(12, 18, 12, 21);
    } else if (element == Element::file_input || element == Element::file_output) {
        p.line(7, 8, 7, 3); p.line(7, 3, 15, 3); p.line(15, 3, 21, 9);
        p.line(21, 9, 21, 21); p.line(21, 21, 7, 21); p.line(7, 21, 7, 16);
        p.line(15, 3, 15, 9); p.line(15, 9, 21, 9); p.line(2, 12, 15, 12);
        if (element == Element::file_input) { p.line(11, 8, 15, 12); p.line(11, 16, 15, 12); }
        else { p.line(6, 8, 2, 12); p.line(6, 16, 2, 12); }
    }
    p.draw();
}
} // namespace gatehaven::ui
