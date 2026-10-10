#pragma once
#include "font.hpp"
#include "symbols.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <numbers>

namespace gatehaven::ui {
inline void rounded(SDL_Renderer* renderer, float x, float y, float width, float height,
                    SDL_Color color, float radius = 7) {
    constexpr int segments = 6, perimeter = (segments + 1) * 4;
    std::array<SDL_Vertex, perimeter + 1> vertices{};
    std::array<int, perimeter * 3> indices{};
    const SDL_FColor c{color.r / 255.0F, color.g / 255.0F, color.b / 255.0F, color.a / 255.0F};
    vertices[0] = {{x + width / 2, y + height / 2}, c, {}};
    const std::array<SDL_FPoint, 4> centers{{{x + width - radius, y + radius},
        {x + width - radius, y + height - radius}, {x + radius, y + height - radius}, {x + radius, y + radius}}};
    for (int corner = 0; corner < 4; ++corner) {
        for (int step = 0; step <= segments; ++step) {
            const auto index = corner * (segments + 1) + step;
            const float angle = (static_cast<float>(corner - 1) + static_cast<float>(step) / segments) * std::numbers::pi_v<float> / 2;
            const auto center = centers[static_cast<std::size_t>(corner)];
            vertices[static_cast<std::size_t>(index + 1)] = {{center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius}, c, {}};
            indices[static_cast<std::size_t>(index * 3)] = 0;
            indices[static_cast<std::size_t>(index * 3 + 1)] = index + 1;
            indices[static_cast<std::size_t>(index * 3 + 2)] = (index + 1) % perimeter + 1;
        }
    }
    SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()), indices.data(), static_cast<int>(indices.size()));
}

inline void panel(SDL_Renderer* renderer, float x, float y, float w, float h, SDL_Color fill, SDL_Color border, float radius = 7) {
    rounded(renderer, x, y, w, h, border, radius);
    rounded(renderer, x + 1, y + 1, w - 2, h - 2, fill, radius - 1);
}

inline void centered(SDL_Renderer* renderer, float x, float y, float w, float h,
                     std::string_view value, SDL_Color color, float size = 14, Weight weight = Weight::semibold) {
    label(renderer, x + (w - text_width(value, size, weight)) / 2, y + (h - size * 1.2F) / 2 - 1,
          value, color, size, weight);
}

enum class Icon { wire, crossing, source, signal, and_gate, or_gate, nand_gate, nor_gate,
    positive_relay, negative_relay, screen, file_input, file_output, select, pan, erase, interact, play, pause, logo };

inline void icon(SDL_Renderer* renderer, Icon kind, float x, float y, float size, SDL_Color color) {
    constexpr std::array components{Element::wire, Element::crossing, Element::source, Element::signal,
        Element::and_gate, Element::or_gate, Element::nand_gate, Element::nor_gate, Element::positive_relay,
        Element::negative_relay, Element::screen, Element::file_input, Element::file_output};
    const auto index = static_cast<std::size_t>(kind);
    if (index < components.size()) {
        component_symbol(renderer, components[index], x, y, size, color, Direction::east, components[index] == Element::negative_relay); return;
    }
    const float scale = size / 24;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    const auto stroke = [&](float x1, float y1, float x2, float y2) {
        const float dx = x2 - x1, dy = y2 - y1, length = std::sqrt(dx * dx + dy * dy);
        if (length == 0) return;
        const float ox = -dy / length * .85F, oy = dx / length * .85F;
        const SDL_FColor c{color.r / 255.0F, color.g / 255.0F, color.b / 255.0F, color.a / 255.0F};
        const SDL_Vertex vertices[]{{{x + (x1 + ox) * scale, y + (y1 + oy) * scale}, c, {}},
            {{x + (x2 + ox) * scale, y + (y2 + oy) * scale}, c, {}},
            {{x + (x2 - ox) * scale, y + (y2 - oy) * scale}, c, {}},
            {{x + (x1 - ox) * scale, y + (y1 - oy) * scale}, c, {}}};
        constexpr int indices[]{0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(renderer, nullptr, vertices, 4, indices, 6);
    };
    switch (kind) {
    case Icon::logo: {
        stroke(5, 4, 12, 4); stroke(5, 4, 5, 20); stroke(5, 20, 12, 20);
        for (int i = 0; i < 12; ++i) {
            const float a = -std::numbers::pi_v<float> / 2 + static_cast<float>(i) * std::numbers::pi_v<float> / 12;
            const float b = a + std::numbers::pi_v<float> / 12;
            stroke(12 + 7 * std::cos(a), 12 + 8 * std::sin(a), 12 + 7 * std::cos(b), 12 + 8 * std::sin(b));
        }
        stroke(1, 8, 5, 8); stroke(1, 16, 5, 16); stroke(19, 12, 23, 12);
        break;
    }
    case Icon::wire: case Icon::crossing: case Icon::signal:
    case Icon::source: case Icon::and_gate: case Icon::or_gate: case Icon::nand_gate: case Icon::nor_gate:
    case Icon::positive_relay: case Icon::negative_relay: case Icon::screen: case Icon::file_input: case Icon::file_output:
        break; // Component symbols were drawn above.
    case Icon::select: stroke(5, 3, 5, 20); stroke(5, 3, 18, 14); stroke(18, 14, 11, 14); stroke(11, 14, 5, 20); break;
    case Icon::pan: stroke(12, 2, 12, 22); stroke(2, 12, 22, 12); stroke(9, 5, 12, 2); stroke(15, 5, 12, 2); stroke(9, 19, 12, 22); stroke(15, 19, 12, 22); stroke(5, 9, 2, 12); stroke(5, 15, 2, 12); stroke(19, 9, 22, 12); stroke(19, 15, 22, 12); break;
    case Icon::erase: stroke(3, 14, 13, 4); stroke(13, 4, 21, 12); stroke(21, 12, 13, 20); stroke(13, 20, 9, 20); stroke(9, 20, 3, 14); stroke(7, 10, 16, 18); break;
    case Icon::interact: stroke(8, 15, 8, 5); stroke(8, 5, 11, 5); stroke(11, 5, 11, 12); stroke(11, 12, 19, 13); stroke(19, 13, 18, 21); stroke(18, 21, 9, 21); stroke(9, 21, 4, 15); stroke(4, 15, 8, 15); break;
    case Icon::play: stroke(7, 4, 7, 20); stroke(7, 4, 20, 12); stroke(20, 12, 7, 20); break;
    case Icon::pause: stroke(8, 5, 8, 19); stroke(16, 5, 16, 19); break;
    }
}
} // namespace gatehaven::ui
