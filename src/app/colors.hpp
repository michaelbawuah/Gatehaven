#pragma once
#include <SDL3/SDL.h>

namespace gatehaven::ui {
struct ColorScheme {
    SDL_Color ink, muted, teal, orange, paper, white, border;
};
inline constexpr ColorScheme standard_colors{
    {28, 43, 54, 255}, {86, 103, 116, 255}, {8, 119, 99, 255},
    {191, 78, 28, 255}, {247, 249, 250, 255}, {255, 255, 255, 255}, {220, 228, 232, 255}};
inline constexpr ColorScheme high_contrast_colors{
    {0, 0, 0, 255}, {40, 40, 40, 255}, {0, 85, 68, 255},
    {132, 49, 0, 255}, {255, 255, 255, 255}, {255, 255, 255, 255}, {0, 0, 0, 255}};
}
