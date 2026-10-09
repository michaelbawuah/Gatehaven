#pragma once
#include "gatehaven/touch.hpp"
#include <cmath>
namespace gatehaven {
class TapSequence {
public:
    unsigned begin(std::uint64_t device, std::uint64_t time, TouchPoint point) {
        const bool continuation = ready_ && device == device_ && time >= released_ && time - released_ <= interval && near(point, previous_);
        count_ = continuation ? count_ % 3 + 1 : 1;
        ready_ = false; eligible_ = true; device_ = device; started_ = time; origin_ = point;
        return count_;
    }
    void move(TouchPoint point) { if (!near(point, origin_)) eligible_ = false; }
    void end(std::uint64_t time, TouchPoint point) {
        move(point); ready_ = eligible_ && time >= started_ && time - started_ <= interval;
        released_ = time; previous_ = point;
    }
    void clear() { ready_ = eligible_ = false; count_ = 0; }
private:
    static constexpr std::uint64_t interval = 500'000'000;
    static bool near(TouchPoint a, TouchPoint b) { return std::hypot(a.x - b.x, a.y - b.y) <= 6; }
    bool ready_{}, eligible_{};
    unsigned count_{};
    std::uint64_t device_{}, started_{}, released_{};
    TouchPoint origin_, previous_;
};
}
