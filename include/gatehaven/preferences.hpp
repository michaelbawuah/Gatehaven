#pragma once
#include "gatehaven/types.hpp"
#include <expected>
#include <string>

namespace gatehaven {
enum class ToolKind { pencil, eraser, panner, selector, interactor };
struct InputTool {
    ToolKind kind{ToolKind::pencil};
    Element element{Element::wire};
    bool operator==(const InputTool&) const = default;
};
struct Preferences {
    std::array<InputTool, 6> bindings{{{ToolKind::pencil}, {ToolKind::eraser}, {ToolKind::panner},
        {ToolKind::selector}, {ToolKind::interactor}, {ToolKind::pencil}}};
    unsigned speed{5};
    bool beginner{};
    bool high_contrast{};
    bool operator==(const Preferences&) const = default;
};
[[nodiscard]] std::string encode_preferences(const Preferences& preferences);
[[nodiscard]] std::expected<Preferences, std::string> decode_preferences(std::string_view text);
}
