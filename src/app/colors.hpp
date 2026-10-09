#pragma once
#include <SDL3/SDL.h>

namespace gatehaven::ui {
struct ColorScheme {
    SDL_Color ink, muted, teal, orange, paper, white, border;
};
inline constexpr ColorScheme standard_colors{
    {31, 45, 64, 255}, {87, 105, 122, 255}, {0, 133, 111, 255},
    {220, 98, 40, 255}, {246, 248, 248, 255}, {255, 255, 255, 255}, {217, 225, 229, 255}};
inline constexpr ColorScheme high_contrast_colors{
    {0, 0, 0, 255}, {40, 40, 40, 255}, {0, 85, 68, 255},
    {132, 49, 0, 255}, {255, 255, 255, 255}, {255, 255, 255, 255}, {0, 0, 0, 255}};
}
