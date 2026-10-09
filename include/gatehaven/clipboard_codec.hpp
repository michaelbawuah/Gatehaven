#pragma once

#include "gatehaven/editor.hpp"

#include <string_view>

namespace gatehaven {

inline constexpr std::size_t max_clipboard_cells = 1'000'000;
inline constexpr std::size_t max_clipboard_bytes = 40 + 9 * max_clipboard_cells;

// A versioned, bounded wire format. The checksum detects accidental corruption;
// it is not an authentication mechanism.
[[nodiscard]] std::expected<std::string, std::string> encode_stamp(const Stamp& stamp);
[[nodiscard]] std::expected<Stamp, std::string> decode_stamp(std::string_view bytes);

} // namespace gatehaven
