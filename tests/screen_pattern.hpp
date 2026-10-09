#pragma once
#include <cstdint>
// Identical external events in both peers; coordinates are the first cell in y/x order.
inline bool screen_pattern(std::uint64_t tick, std::int32_t x, std::int32_t y) {
    const auto seed = static_cast<std::uint32_t>(x) * 31U + static_cast<std::uint32_t>(y) * 17U;
    return ((tick + seed) % 7U) < 3U;
}
