#pragma once
#include <algorithm>
#include <cmath>

namespace gatehaven {
// UI wall-clock scheduling only. Simulation state remains entirely integer based.
class TickSchedule {
public:
    [[nodiscard]] unsigned due(double elapsed, unsigned speed) {
        if (!std::isfinite(elapsed) || elapsed < 0) elapsed = 0;
        if (speed < 1 || speed > 1000) { reset(); return 0; }
        const double ticks = fraction_ + std::clamp(elapsed, 0.0, 0.25) * speed;
        const auto whole = static_cast<unsigned>(std::floor(ticks + 1e-9));
        fraction_ = std::max(0.0, ticks - whole);
        return whole;
    }
    void reset() { fraction_ = 0; }
private:
    double fraction_{};
};
}
