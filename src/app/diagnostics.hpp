#pragma once
#include "gatehaven/version.hpp"
#include <SDL3/SDL.h>
#include <cmath>
#include <ostream>
#include <stdexcept>
#include <string_view>

namespace gatehaven::ui {
inline void json_string(std::ostream& out, std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (const auto byte : value) {
        const auto c = static_cast<unsigned char>(byte);
        if (c == '"' || c == '\\') out << '\\' << byte;
        else if (c < 32) out << "\\u00" << hex[c >> 4] << hex[c & 15];
        else out << byte;
    }
    out << '"';
}

inline void display_information(std::ostream& out, SDL_Window* window, SDL_Renderer* renderer) {
    int width{}, height{}, pixels_w{}, pixels_h{}, output_w{}, output_h{}, vsync{};
    if (!SDL_GetWindowSize(window, &width, &height) || !SDL_GetWindowSizeInPixels(window, &pixels_w, &pixels_h) ||
        !SDL_GetRenderOutputSize(renderer, &output_w, &output_h)) throw std::runtime_error(SDL_GetError());
    const auto* driver = SDL_GetCurrentVideoDriver();
    const auto* name = SDL_GetRendererName(renderer);
    const auto density = SDL_GetWindowPixelDensity(window);
    const auto scale = SDL_GetWindowDisplayScale(window);
    const auto* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
    const auto number = [&](float value) { if (std::isfinite(value) && value > 0) out << value; else out << "null"; };
    out << "{\"video_driver\":"; json_string(out, driver ? driver : "unknown");
    out << ",\"renderer\":"; json_string(out, name ? name : "unknown");
    out << ",\"sdl_version\":" << SDL_GetVersion()
        << ",\"window_width\":" << width << ",\"window_height\":" << height
        << ",\"pixel_width\":" << pixels_w << ",\"pixel_height\":" << pixels_h
        << ",\"output_width\":" << output_w << ",\"output_height\":" << output_h
        << ",\"logical_width\":1280,\"logical_height\":800,\"pixel_density\":";
    number(density); out << ",\"display_scale\":"; number(scale);
    out << ",\"refresh_hz\":"; number(mode ? mode->refresh_rate : 0);
    out << ",\"vsync\":";
    if (SDL_GetRenderVSync(renderer, &vsync)) out << vsync; else out << "null";
    out << '}';
}

inline void diagnostics(std::ostream& out, SDL_Window* window, SDL_Renderer* renderer) {
    out << "{\"schema\":1,\"product\":\"Gatehaven\",\"version\":"; json_string(out, version);
    out << ",\"source_revision\":"; json_string(out, source_revision);
    out << ",\"build_configuration\":"; json_string(out, GATEHAVEN_APP_BUILD_CONFIG);
    out << ",\"build_info\":"; json_string(out, build_information());
    out << ",\"display\":"; display_information(out, window, renderer);
    out << ",\"physical_device_verified\":false,\"limits\":["
           "\"Backend and scaling observations do not establish physical-device acceptance\"]}\n";
}
}
