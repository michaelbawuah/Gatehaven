#pragma once

#include <SDL3/SDL.h>
#include <array>
#include <string>
#include <string_view>

namespace gatehaven::ui {
enum class Weight { regular = 400, semibold = 600 };
// Owns renderer-specific textures. Construct after the renderer and destroy first.
class FontAtlas {
public:
    explicit FontAtlas(SDL_Renderer* renderer);
    ~FontAtlas();
    FontAtlas(const FontAtlas&) = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;
private:
    SDL_Renderer* renderer_;
    std::array<SDL_Texture*, 3> textures_{};
};
float text_width(std::string_view value, float size = 14, Weight weight = Weight::regular);
std::string ellipsize(std::string_view value, float max_width, float size = 14, Weight weight = Weight::regular);
void label(SDL_Renderer* renderer, float x, float y, std::string_view value,
           SDL_Color color, float size = 14, Weight weight = Weight::regular);
void text(SDL_Renderer* renderer, float x, float y, std::string_view value,
          SDL_Color color, float scale = 2.0F);
}
