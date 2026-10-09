#pragma once
#include <cstdint>
#include <map>
#include <utility>

namespace gatehaven {
using TouchId = std::pair<std::uint64_t, std::uint64_t>; // Device, finger.
struct TouchPoint { double x{}, y{}; };
enum class TouchAction { none, begin, move, end, cancel, navigate };
struct TouchUpdate {
    TouchAction action{};
    TouchPoint before, after;
    double zoom{1};
};
class TouchGesture {
public:
    [[nodiscard]] TouchUpdate down(TouchId id, TouchPoint point);
    [[nodiscard]] TouchUpdate move(TouchId id, TouchPoint point);
    [[nodiscard]] TouchUpdate up(TouchId id, TouchPoint point);
    void clear() { contacts_.clear(); navigation_ = false; }
    [[nodiscard]] std::size_t size() const { return contacts_.size(); }
private:
    std::map<TouchId, TouchPoint> contacts_;
    bool navigation_{};
};
}
