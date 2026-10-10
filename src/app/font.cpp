#include "font.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace gatehaven::ui {
namespace {
struct Glyph {
    std::uint32_t code;
    int weight, x, y, width, height, left, top;
    float advance;
};
#include "font_atlas.inc"

std::unordered_map<SDL_Renderer*, std::array<SDL_Texture*, 3>> textures;

std::uint32_t next(std::string_view value, std::size_t& offset) {
    const auto lead = static_cast<unsigned char>(value[offset++]);
    if (lead < 0x80) return lead;
    const unsigned count = lead >= 0xf0 && lead <= 0xf4 ? 3 : lead >= 0xe0 && lead <= 0xef ? 2 : lead >= 0xc2 && lead <= 0xdf ? 1 : 0;
    if (!count || value.size() - offset < count) return '?';
    std::uint32_t result = lead & (0x7fU >> (count + 1));
    for (unsigned i = 0; i < count; ++i) {
        const auto byte = static_cast<unsigned char>(value[offset]);
        if ((byte & 0xc0) != 0x80) return '?';
        result = (result << 6) | (byte & 0x3f); ++offset;
    }
    if ((count == 1 && result < 0x80) || (count == 2 && result < 0x800) ||
        (count == 3 && result < 0x10000) || result > 0x10ffff || (result >= 0xd800 && result <= 0xdfff)) return '?';
    return result;
}

const Glyph& glyph(std::uint32_t code, Weight weight) {
    const auto begin = std::begin(glyphs) + (weight == Weight::semibold ? std::size(glyphs) / 2 : 0);
    const auto end = begin + std::size(glyphs) / 2;
    const auto found = std::lower_bound(begin, end, code, [](const Glyph& g, auto c) { return g.code < c; });
    if (found != end && found->code == code) return *found;
    return begin['?' - ' '];
}
}

FontAtlas::FontAtlas(SDL_Renderer* renderer) : renderer_(renderer) {
    if (textures.contains(renderer)) throw std::runtime_error("Font atlas already initialized");
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
        SDL_CreateSurface(atlas_width, atlas_height, SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
    if (!surface) throw std::runtime_error(SDL_GetError());
    std::size_t pixel = 0;
    for (std::size_t i = 0; i < std::size(atlas_runs); i += 2) {
        for (unsigned run = 0; run < atlas_runs[i]; ++run) {
            if (pixel >= static_cast<std::size_t>(atlas_width * atlas_height)) throw std::runtime_error("Invalid font atlas");
            auto* out = static_cast<unsigned char*>(surface->pixels) +
                (pixel / atlas_width) * static_cast<std::size_t>(surface->pitch) + (pixel % atlas_width) * 4;
            out[0] = out[1] = out[2] = 255; out[3] = atlas_runs[i + 1]; ++pixel;
        }
    }
    if (pixel != static_cast<std::size_t>(atlas_width * atlas_height)) throw std::runtime_error("Truncated font atlas");
    // Area-filtered levels prevent thin strokes disappearing when a large atlas
    // is minified on 1x displays. Keep the 64 px level for Retina and larger type.
    try {
        for (std::size_t level = 0; level < textures_.size(); ++level) {
            auto*& texture = textures_[level];
            texture = SDL_CreateTextureFromSurface(renderer, surface.get());
            if (!texture || !SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) ||
                !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR)) throw std::runtime_error(SDL_GetError());
            if (level + 1 == textures_.size()) break;
            std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> smaller(
                SDL_CreateSurface((surface->w + 1) / 2, (surface->h + 1) / 2, SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
            if (!smaller) throw std::runtime_error(SDL_GetError());
            for (int y = 0; y < smaller->h; ++y) for (int x = 0; x < smaller->w; ++x) {
                unsigned alpha = 0;
                for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) {
                    const int sx = x * 2 + dx, sy = y * 2 + dy;
                    if (sx < surface->w && sy < surface->h) alpha += static_cast<unsigned char*>(surface->pixels)[sy * surface->pitch + sx * 4 + 3];
                }
                auto* out = static_cast<unsigned char*>(smaller->pixels) + y * smaller->pitch + x * 4;
                out[0] = out[1] = out[2] = 255; out[3] = static_cast<unsigned char>((alpha + 2) / 4);
            }
            surface = std::move(smaller);
        }
        textures.emplace(renderer, textures_);
    } catch (...) {
        for (auto* texture : textures_) SDL_DestroyTexture(texture);
        throw;
    }
}

FontAtlas::~FontAtlas() { textures.erase(renderer_); for (auto* texture : textures_) SDL_DestroyTexture(texture); }

float text_width(std::string_view value, float size, Weight weight) {
    float result = 0;
    for (std::size_t offset = 0; offset < value.size();) result += glyph(next(value, offset), weight).advance;
    return result * size / atlas_size;
}

std::string ellipsize(std::string_view value, float max_width, float size, Weight weight) {
    if (text_width(value, size, weight) <= max_width) return std::string(value);
    const float dots = text_width("…", size, weight);
    if (dots > max_width) return {};
    float width = 0;
    std::size_t end = 0;
    for (std::size_t offset = 0; offset < value.size();) {
        width += glyph(next(value, offset), weight).advance * size / atlas_size;
        if (width + dots > max_width) break;
        end = offset;
    }
    return std::string(value.substr(0, end)) + "…";
}

void label(SDL_Renderer* renderer, float x, float y, std::string_view value, SDL_Color color, float size, Weight weight) {
    const auto found = textures.find(renderer);
    if (found == textures.end()) throw std::runtime_error("Font atlas is not initialized");
    float scale_x = 1, scale_y = 1;
    SDL_GetRenderScale(renderer, &scale_x, &scale_y);
    int logical_width = 0, logical_height = 0;
    SDL_RendererLogicalPresentation mode{};
    SDL_GetRenderLogicalPresentation(renderer, &logical_width, &logical_height, &mode);
    SDL_FRect presentation{};
    if (logical_height > 0 && SDL_GetRenderLogicalPresentationRect(renderer, &presentation)) scale_y *= presentation.h / static_cast<float>(logical_height);
    const auto level = size * scale_y <= 16 ? 2U : size * scale_y <= 32 ? 1U : 0U;
    const float divisor = static_cast<float>(1U << level);
    auto* texture = found->second[level];
    SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(texture, color.a);
    const float factor = size / atlas_size;
    const float baseline = y + size;
    for (std::size_t offset = 0; offset < value.size();) {
        const auto& g = glyph(next(value, offset), weight);
        if (g.width && g.height) {
            const SDL_FRect source{static_cast<float>(g.x) / divisor, static_cast<float>(g.y) / divisor, static_cast<float>(g.width) / divisor, static_cast<float>(g.height) / divisor};
            const SDL_FRect target{x + static_cast<float>(g.left) * factor, baseline + static_cast<float>(g.top) * factor,
                static_cast<float>(g.width) * factor, static_cast<float>(g.height) * factor};
            SDL_RenderTexture(renderer, texture, &source, &target);
        }
        x += g.advance * factor;
    }
}

void text(SDL_Renderer* renderer, float x, float y, std::string_view value, SDL_Color color, float scale) {
    label(renderer, x, y, value, color, scale * 9.0F);
}
} // namespace gatehaven::ui
