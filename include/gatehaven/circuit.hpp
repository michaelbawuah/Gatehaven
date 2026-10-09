#pragma once

#include "gatehaven/types.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <vector>

namespace gatehaven {

class Circuit {
public:
    [[nodiscard]] Element at(Point point) const;
    bool set(Point point, Element element);
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] std::size_t count(Element element) const noexcept {
        const auto index = static_cast<std::size_t>(element);
        return index < counts_.size() ? counts_[index] : 0;
    }
    void clear();
    [[nodiscard]] std::vector<Cell> cells() const;
    [[nodiscard]] std::vector<Cell> cells_in(Bounds bounds) const;
    [[nodiscard]] std::optional<Bounds> bounds() const;
    bool operator==(const Circuit&) const = default;

private:
    // Two ordered indexes permit viewport queries without scanning distant columns.
    std::map<Coordinate, std::map<Coordinate, Element>> rows_;
    std::size_t size_{};
    std::array<std::size_t, element_names.size()> counts_{};
};

} // namespace gatehaven
