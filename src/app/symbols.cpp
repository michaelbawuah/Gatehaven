#include "symbols.hpp"

#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace gatehaven::ui {
namespace {
#include "symbol_atlas.inc"
std::unordered_map<SDL_Renderer*, std::array<SDL_Texture*, 6>> atlases;

int symbol_index(Element element, bool conducting) {
    switch (element) {
    case Element::wire: return 0;
    case Element::crossing: return 1;
    case Element::source: return 2;
    case Element::signal: return 3;
    case Element::and_gate: return 4;
    case Element::or_gate: return 5;
    case Element::nand_gate: return 6;
    case Element::nor_gate: return 7;
    case Element::positive_relay: case Element::negative_relay: return conducting ? 9 : 8;
    case Element::screen: return 10;
    case Element::file_input: return 11;
    case Element::file_output: return 12;
    case Element::empty: return -1;
    }
    return -1;
}
}

SymbolAtlas::SymbolAtlas(SDL_Renderer* renderer) : renderer_(renderer) {
    if (atlases.contains(renderer)) throw std::runtime_error("Component atlas already initialized");
    using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
    Surface surface(SDL_CreateSurface(symbol_atlas_size, symbol_atlas_size, SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
    if (!surface) throw std::runtime_error(SDL_GetError());
    std::size_t pixel = 0;
    constexpr auto pixel_count = static_cast<std::size_t>(symbol_atlas_size * symbol_atlas_size);
    for (const auto entry : symbol_runs) {
        const auto count = entry >> 8;
        if (count == 0 || count > pixel_count - pixel) throw std::runtime_error("Invalid component atlas");
        for (std::uint32_t i = 0; i < count; ++i, ++pixel) {
            auto* out = static_cast<unsigned char*>(surface->pixels) +
                (pixel / symbol_atlas_size) * static_cast<std::size_t>(surface->pitch) + (pixel % symbol_atlas_size) * 4;
            out[0] = out[1] = out[2] = 255; out[3] = static_cast<unsigned char>(entry & 255U);
        }
    }
    if (pixel != pixel_count) throw std::runtime_error("Truncated component atlas");
    try {
        for (std::size_t level = 0; level < textures_.size(); ++level) {
            auto*& texture = textures_[level];
            texture = SDL_CreateTextureFromSurface(renderer, surface.get());
            if (!texture || !SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) ||
                !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR)) throw std::runtime_error(SDL_GetError());
            if (level + 1 == textures_.size()) break;
            Surface smaller(SDL_CreateSurface(surface->w / 2, surface->h / 2, SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
            if (!smaller) throw std::runtime_error(SDL_GetError());
            for (int y = 0; y < smaller->h; ++y) for (int x = 0; x < smaller->w; ++x) {
                unsigned alpha = 0;
                for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx)
                    alpha += static_cast<unsigned char*>(surface->pixels)[(y * 2 + dy) * surface->pitch + (x * 2 + dx) * 4 + 3];
                auto* out = static_cast<unsigned char*>(smaller->pixels) + y * smaller->pitch + x * 4;
                out[0] = out[1] = out[2] = 255; out[3] = static_cast<unsigned char>((alpha + 2) / 4);
            }
            surface = std::move(smaller);
        }
        atlases.emplace(renderer, textures_);
    } catch (...) {
        for (auto* texture : textures_) SDL_DestroyTexture(texture);
        throw;
    }
}

SymbolAtlas::~SymbolAtlas() { atlases.erase(renderer_); for (auto* texture : textures_) SDL_DestroyTexture(texture); }

void component_symbol(SDL_Renderer* renderer, Element element, float x, float y, float size,
                      SDL_Color color, Direction direction, bool conducting) {
    const auto index = symbol_index(element, conducting);
    if (index < 0 || size <= 0) return;
    const auto found = atlases.find(renderer);
    if (found == atlases.end()) throw std::runtime_error("Component atlas is not initialized");
    float scale_x = 1, scale_y = 1;
    SDL_GetRenderScale(renderer, &scale_x, &scale_y);
    int logical_width = 0, logical_height = 0;
    SDL_RendererLogicalPresentation mode{};
    SDL_GetRenderLogicalPresentation(renderer, &logical_width, &logical_height, &mode);
    SDL_FRect presentation{};
    if (logical_height > 0 && SDL_GetRenderLogicalPresentationRect(renderer, &presentation))
        scale_y *= presentation.h / static_cast<float>(logical_height);
    std::size_t level = 0;
    while (level + 1 < found->second.size() && size * scale_y <= static_cast<float>(symbol_tile_size >> (level + 1))) ++level;
    const float tile = static_cast<float>(symbol_tile_size >> level);
    const SDL_FRect source{static_cast<float>(index % 4) * tile, static_cast<float>(index / 4) * tile, tile, tile};
    const SDL_FRect destination{x, y, size, size};
    auto* texture = found->second[level];
    SDL_SetTextureColorMod(texture, color.r, color.g, color.b); SDL_SetTextureAlphaMod(texture, color.a);
    const auto turns = (static_cast<unsigned>(direction) + 3U) % 4U;
    SDL_RenderTextureRotated(renderer, texture, &source, &destination, turns * 90.0, nullptr, SDL_FLIP_NONE);
    // Polarity stays distinct while the downloaded relay contacts change state.
    if (is_relay(element)) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        const float width = std::max(1.0F, size * .035F);
        const SDL_FRect minus{x + size * .34F, y + size * .045F, size * .18F, width};
        SDL_RenderFillRect(renderer, &minus);
        if (element == Element::positive_relay) {
            const SDL_FRect plus{x + size * .43F - width / 2, y - size * .025F, width, size * .18F};
            SDL_RenderFillRect(renderer, &plus);
        }
    }
}
} // namespace gatehaven::ui
