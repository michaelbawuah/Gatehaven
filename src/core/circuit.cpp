#include "gatehaven/circuit.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace gatehaven {

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
    if (at(point) == element) return false;
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
    return true;
}

void Circuit::clear() {
    rows_.clear();
    size_ = 0;
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
    if (bounds.min.x > bounds.max.x || bounds.min.y > bounds.max.y) return result;
    for (auto row = rows_.lower_bound(bounds.min.y);
         row != rows_.end() && row->first <= bounds.max.y; ++row) {
        for (auto cell = row->second.lower_bound(bounds.min.x);
             cell != row->second.end() && cell->first <= bounds.max.x; ++cell) {
            result.push_back({{cell->first, row->first}, cell->second});
        }
    }
    return result;
}

std::optional<Bounds> Circuit::bounds() const {
    if (empty()) return std::nullopt;
    auto min_x = std::numeric_limits<Coordinate>::max();
    auto max_x = std::numeric_limits<Coordinate>::min();
    for (const auto& [y, row] : rows_) {
        static_cast<void>(y);
        min_x = std::min(min_x, row.begin()->first);
        max_x = std::max(max_x, row.rbegin()->first);
    }
    return Bounds{{min_x, rows_.begin()->first}, {max_x, rows_.rbegin()->first}};
}

} // namespace gatehaven
