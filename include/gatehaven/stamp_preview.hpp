#pragma once
#include "gatehaven/editor.hpp"
#include <map>
namespace gatehaven {
// Build once when the clipboard or transform changes; queries skip distant rows/columns.
class StampPreview {
public:
    void reset(const Stamp& stamp) {
        rows_.clear();
        for (const auto& cell : stamp.cells) rows_[cell.y][cell.x] = cell;
    }
    template<class Visitor> void visit(Point origin, Bounds clip, Visitor&& visitor) const {
        const auto left = static_cast<std::int64_t>(clip.min.x) - origin.x;
        const auto right = static_cast<std::int64_t>(clip.max.x) - origin.x;
        const auto top = static_cast<std::int64_t>(clip.min.y) - origin.y;
        const auto bottom = static_cast<std::int64_t>(clip.max.y) - origin.y;
        for (auto row = rows_.lower_bound(top); row != rows_.end() && row->first <= bottom; ++row) {
            for (auto cell = row->second.lower_bound(left); cell != row->second.end() && cell->first <= right; ++cell) {
                const auto point = translated(origin, cell->first, row->first);
                if (point) visitor(Cell{*point, cell->second.element, cell->second.state});
            }
        }
    }
private:
    std::map<std::int64_t, std::map<std::int64_t, StampCell>> rows_;
};
}
