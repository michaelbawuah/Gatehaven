#include "font.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gatehaven::ui {
namespace {
using Glyph = std::array<std::uint8_t, 7>;
// Small geometric bitmap alphabet drawn specifically for the prototype UI.
Glyph glyph(char c) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    switch (c) {
    case 'A': return {14,17,17,31,17,17,17};
    case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14};
    case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31};
    case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15};
    case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {31,4,4,4,4,4,31};
    case 'J': return {7,2,2,2,18,18,12};
    case 'K': return {17,18,20,24,20,18,17};
    case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17};
    case 'N': return {17,25,25,21,19,19,17};
    case 'O': return {14,17,17,17,17,17,14};
    case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13};
    case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30};
    case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14};
    case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,27,17};
    case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4};
    case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14};
    case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31};
    case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2};
    case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14};
    case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14};
    case '9': return {14,17,17,15,1,1,14};
    case '+': return {0,4,4,31,4,4,0};
    case '-': return {0,0,0,31,0,0,0};
    case '/': return {1,1,2,4,8,16,16};
    case ':': return {0,4,4,0,4,4,0};
    case '.': return {0,0,0,0,0,4,4};
    case ',': return {0,0,0,0,4,4,8};
    case '[': return {14,8,8,8,8,8,14};
    case ']': return {14,2,2,2,2,2,14};
    case '(': return {2,4,8,8,8,4,2};
    case ')': return {8,4,2,2,2,4,8};
    case '>': return {16,8,4,2,4,8,16};
    case '<': return {1,2,4,8,4,2,1};
    case '=': return {0,31,0,31,0,0,0};
    case '*': return {0,21,14,31,14,21,0};
    case ' ': return {};
    default: return {14,17,1,2,4,0,4};
    }
}
} // namespace

void text(SDL_Renderer* renderer, float x, float y, std::string_view value,
          SDL_Color color, float scale) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (const char ch : value) {
        const auto rows = glyph(ch);
        for (std::size_t row = 0; row < rows.size(); ++row) {
            for (unsigned column = 0; column < 5; ++column) {
                if ((rows[row] & (1U << (4U - column))) == 0) continue;
                const float left = std::round(x + static_cast<float>(column) * scale);
                const float top = std::round(y + static_cast<float>(row) * scale);
                const float right = std::round(x + static_cast<float>(column + 1) * scale);
                const float bottom = std::round(y + static_cast<float>(row + 1) * scale);
                const SDL_FRect pixel{left, top, std::max(1.0F, right - left), std::max(1.0F, bottom - top)};
                SDL_RenderFillRect(renderer, &pixel);
            }
        }
        x += 6 * scale;
    }
}
} // namespace gatehaven::ui
