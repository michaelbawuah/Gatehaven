#pragma once

#include "gatehaven/circuit.hpp"

#include <cstdint>
#include <map>
#include <span>

namespace gatehaven {

struct Power {
    Element element{Element::empty};
    // N=1, E=2, S=4, W=8. Crossing wires may power just one axis.
    std::uint8_t ports{};
    bool operator==(const Power&) const = default;
};

class Simulation {
public:
    void step(const Circuit& circuit);
    void reset();
    void invalidate(std::span<const Point> points);
    [[nodiscard]] bool powered(Point point) const { return ports(point) != 0; }
    [[nodiscard]] std::uint8_t ports(Point point) const;
    [[nodiscard]] std::uint64_t ticks() const noexcept { return ticks_; }
    [[nodiscard]] std::size_t powered_count() const;
    [[nodiscard]] const std::map<Point, Power>& snapshot() const noexcept { return state_; }

private:
    std::map<Point, Power> state_;
    std::uint64_t ticks_{};
};

} // namespace gatehaven
