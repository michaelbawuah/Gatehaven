#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <tuple>

namespace gatehaven {

using Coordinate = std::int32_t;

struct Point {
    Coordinate x{};
    Coordinate y{};
    bool operator==(const Point&) const = default;
    auto operator<=>(const Point& other) const {
        return std::tie(y, x) <=> std::tie(other.y, other.x);
    }
};

struct Bounds {
    Point min;
    Point max;
    bool operator==(const Bounds&) const = default;
    [[nodiscard]] constexpr bool contains(Point p) const {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
    }
};

enum class Direction : std::uint8_t { north, east, south, west };
inline constexpr std::array directions{Direction::north, Direction::east,
                                       Direction::south, Direction::west};

[[nodiscard]] constexpr std::optional<Point> translated(Point p, std::int64_t dx,
                                                       std::int64_t dy) {
    constexpr auto lo = std::numeric_limits<Coordinate>::min();
    constexpr auto hi = std::numeric_limits<Coordinate>::max();
    // Compare before addition: callers may provide full-width deltas.
    if (dx < static_cast<std::int64_t>(lo) - p.x || dx > static_cast<std::int64_t>(hi) - p.x ||
        dy < static_cast<std::int64_t>(lo) - p.y || dy > static_cast<std::int64_t>(hi) - p.y) {
        return std::nullopt;
    }
    return Point{static_cast<Coordinate>(p.x + dx), static_cast<Coordinate>(p.y + dy)};
}

[[nodiscard]] constexpr std::optional<Point> neighbor(Point p, Direction d) {
    switch (d) {
    case Direction::north: return translated(p, 0, -1);
    case Direction::east: return translated(p, 1, 0);
    case Direction::south: return translated(p, 0, 1);
    case Direction::west: return translated(p, -1, 0);
    }
    return std::nullopt;
}

enum class Element : std::uint8_t {
    empty, wire, crossing, source, signal, positive_relay, negative_relay,
    and_gate, or_gate, nand_gate, nor_gate, screen, file_input, file_output
};

inline constexpr std::array<std::string_view, 14> element_names{
    "empty", "wire", "crossing", "source", "signal", "positive-relay",
    "negative-relay", "and", "or", "nand", "nor", "screen", "file-input", "file-output"
};

[[nodiscard]] constexpr bool is_communicator(Element element) {
    return element == Element::screen || element == Element::file_input || element == Element::file_output;
}

[[nodiscard]] constexpr std::string_view name(Element element) {
    const auto index = static_cast<std::size_t>(element);
    return index < element_names.size() ? element_names[index] : "unknown";
}

[[nodiscard]] constexpr std::optional<Element> parse_element(std::string_view text) {
    for (std::size_t i = 0; i < element_names.size(); ++i) {
        if (element_names[i] == text) return static_cast<Element>(i);
    }
    return std::nullopt;
}

struct Cell {
    Point position;
    Element element;
    bool operator==(const Cell&) const = default;
};

} // namespace gatehaven
