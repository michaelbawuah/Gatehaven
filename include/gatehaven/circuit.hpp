#pragma once

#include "gatehaven/types.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <vector>

namespace gatehaven {

class Circuit {
public:
    Circuit() = default;
    Circuit(const Circuit&) = default;
    Circuit& operator=(const Circuit&) = default;
    Circuit(Circuit&& other) noexcept;
    Circuit& operator=(Circuit&& other) noexcept;
    [[nodiscard]] Element at(Point point) const;
    bool set(Point point, Element element);
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] std::size_t count(Element element) const noexcept {
        const auto index = static_cast<std::size_t>(element);
        return index < counts_.size() ? counts_[index] : 0;
    }
    void clear();
    [[nodiscard]] std::vector<Cell> cells() const;
    [[nodiscard]] std::vector<Cell> cells_in(Bounds bounds) const;
    // The visitor must not mutate this circuit while its indexes are traversed.
    template<class Visitor> void visit(Bounds bounds, Visitor&& visitor) const {
        if (bounds.min.x > bounds.max.x || bounds.min.y > bounds.max.y) return;
        for (auto row = rows_.lower_bound(bounds.min.y); row != rows_.end() && row->first <= bounds.max.y; ++row) {
            for (auto cell = row->second.lower_bound(bounds.min.x); cell != row->second.end() && cell->first <= bounds.max.x; ++cell) {
                visitor(Cell{{cell->first, row->first}, cell->second});
            }
        }
    }
    [[nodiscard]] std::optional<Bounds> bounds() const;
    bool operator==(const Circuit& other) const { return rows_ == other.rows_; }

private:
    // Two ordered indexes permit viewport queries without scanning distant columns.
    std::map<Coordinate, std::map<Coordinate, Element>> rows_;
    std::size_t size_{};
    std::array<std::size_t, element_names.size()> counts_{};
    std::uint64_t revision_{};
    mutable bool bounds_dirty_{true};
    mutable std::optional<Bounds> bounds_cache_;
};

} // namespace gatehaven
