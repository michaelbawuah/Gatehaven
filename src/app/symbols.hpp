#pragma once
#include "gatehaven/circuit.hpp"
#include "gatehaven/connectivity.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>

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

// Renderer-owned images with filtered levels for sharp small icons and Retina zoom.
class SymbolAtlas {
public:
    explicit SymbolAtlas(SDL_Renderer* renderer);
    ~SymbolAtlas();
    SymbolAtlas(const SymbolAtlas&) = delete;
    SymbolAtlas& operator=(const SymbolAtlas&) = delete;
private:
    SDL_Renderer* renderer_;
    std::array<SDL_Texture*, 6> textures_{};
};

void component_symbol(SDL_Renderer* renderer, Element element, float x, float y, float size,
                      SDL_Color color, Direction direction = Direction::east, bool conducting = false);
} // namespace gatehaven::ui
