#include "gatehaven/circuit.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <atomic>
#include <utility>

namespace gatehaven {
namespace {
std::uint64_t next_revision() { static std::atomic<std::uint64_t> value{1}; return value.fetch_add(1, std::memory_order_relaxed); }
}

Circuit::Circuit(Circuit&& other) noexcept { *this = std::move(other); }
Circuit& Circuit::operator=(Circuit&& other) noexcept {
    if (this != &other) {
        rows_ = std::move(other.rows_); other.rows_.clear();
        size_ = std::exchange(other.size_, 0);
        counts_ = std::exchange(other.counts_, {});
        revision_ = std::exchange(other.revision_, 0);
        bounds_dirty_ = other.bounds_dirty_; bounds_cache_ = other.bounds_cache_;
        other.bounds_dirty_ = false; other.bounds_cache_.reset();
    }
    return *this;
}

Element Circuit::at(Point point) const {
    const auto row = rows_.find(point.y);
    if (row == rows_.end()) return Element::empty;
    const auto cell = row->second.find(point.x);
    return cell == row->second.end() ? Element::empty : cell->second;
}

bool Circuit::set(Point point, Element element) {
    if (static_cast<std::size_t>(element) >= element_names.size()) {
        throw std::invalid_argument("Invalid circuit element");
    }
    const auto before = at(point);
    if (before == element) return false;
    if (element == Element::empty) {
        const auto row = rows_.find(point.y);
        row->second.erase(point.x);
        if (row->second.empty()) rows_.erase(row);
        --size_;
    } else {
        auto& row = rows_[point.y];
        if (!row.contains(point.x)) ++size_;
        row.insert_or_assign(point.x, element);
    }
    if (before != Element::empty) --counts_[static_cast<std::size_t>(before)];
    if (element != Element::empty) ++counts_[static_cast<std::size_t>(element)];
    revision_ = next_revision(); bounds_dirty_ = true;
    return true;
}

void Circuit::clear() {
    if (empty()) return;
    rows_.clear();
    size_ = 0;
    counts_.fill(0);
    revision_ = next_revision(); bounds_dirty_ = true;
}

std::vector<Cell> Circuit::cells() const {
    std::vector<Cell> result;
    result.reserve(size_);
    for (const auto& [y, row] : rows_) {
        for (const auto& [x, element] : row) result.push_back({{x, y}, element});
    }
    return result;
}

std::vector<Cell> Circuit::cells_in(Bounds bounds) const {
    std::vector<Cell> result;
    visit(bounds, [&](Cell cell) { result.push_back(cell); });
    return result;
}

std::optional<Bounds> Circuit::bounds() const {
    if (!bounds_dirty_) return bounds_cache_;
    bounds_dirty_ = false;
    if (empty()) { bounds_cache_.reset(); return std::nullopt; }
    auto min_x = std::numeric_limits<Coordinate>::max();
    auto max_x = std::numeric_limits<Coordinate>::min();
    for (const auto& [y, row] : rows_) {
        static_cast<void>(y);
        min_x = std::min(min_x, row.begin()->first);
        max_x = std::max(max_x, row.rbegin()->first);
    }
    bounds_cache_ = Bounds{{min_x, rows_.begin()->first}, {max_x, rows_.rbegin()->first}};
    return bounds_cache_;
}

} // namespace gatehaven
