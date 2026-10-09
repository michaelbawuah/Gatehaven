#pragma once

#include <SDL3/SDL.h>
#include <string_view>

namespace gatehaven::ui {
void text(SDL_Renderer* renderer, float x, float y, std::string_view value,
          SDL_Color color, float scale = 2.0F);
}
