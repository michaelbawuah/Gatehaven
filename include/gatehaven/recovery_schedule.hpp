#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gatehaven {
// Save after two idle seconds or thirty seconds of continuous editing.
class RecoverySchedule {
public:
    [[nodiscard]] bool poll(std::uint64_t revision, bool dirty, double elapsed) {
        if (!std::isfinite(elapsed) || elapsed < 0) elapsed = 0;
        elapsed = std::min(elapsed, 30.0);
        if (!dirty) { age_ = idle_ = retry_ = 0; written_ = seen_ = revision; return false; }
        if (revision == written_) { age_ = idle_ = 0; return false; }
        if (revision != seen_) { seen_ = revision; idle_ = 0; }
        age_ += elapsed; idle_ += elapsed; retry_ = std::max(0.0, retry_ - elapsed);
        return revision != written_ && retry_ == 0 && (idle_ >= 2 || age_ >= 30);
    }
    void written(std::uint64_t revision) { written_ = seen_ = revision; age_ = idle_ = retry_ = 0; }
    void failed() { retry_ = 30; }
private:
    std::uint64_t written_{}, seen_{};
    double age_{}, idle_{}, retry_{};
};
}
