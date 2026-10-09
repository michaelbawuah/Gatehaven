#include "gatehaven/selection.hpp"
#include <algorithm>
#include <map>
#include <utility>

namespace gatehaven {
Selection Selection::rectangle(const Circuit& circuit, Bounds region) {
    Selection result;
    if (region.min.x > region.max.x || region.min.y > region.max.y) return result;
    result.frame_ = region;
    for (const auto& cell : circuit.cells_in(region)) result.points_.insert(cell.position);
    return result;
}
Selection Selection::points(std::set<Point> points) {
    Selection result;
    result.points_ = std::move(points);
    for (const auto p : result.points_) {
        if (!result.frame_) result.frame_ = Bounds{p, p};
        else {
            result.frame_->min.x = std::min(result.frame_->min.x, p.x);
            result.frame_->min.y = std::min(result.frame_->min.y, p.y);
            result.frame_->max.x = std::max(result.frame_->max.x, p.x);
            result.frame_->max.y = std::max(result.frame_->max.y, p.y);
        }
    }
    return result;
}
void Selection::combine(const Selection& other, SelectionMode mode) {
    if (mode == SelectionMode::replace) { *this = other; return; }
    if (mode == SelectionMode::subtract) {
        for (const auto point : other.points_) points_.erase(point);
        *this = points(std::move(points_));
        return;
    }
    points_.insert(other.points_.begin(), other.points_.end());
    if (!other.frame_) return;
    if (!frame_) { frame_ = other.frame_; return; }
    frame_->min.x = std::min(frame_->min.x, other.frame_->min.x);
    frame_->min.y = std::min(frame_->min.y, other.frame_->min.y);
    frame_->max.x = std::max(frame_->max.x, other.frame_->max.x);
    frame_->max.y = std::max(frame_->max.y, other.frame_->max.y);
}
std::vector<Cell> Selection::cells(const Circuit& circuit) const {
    std::vector<Cell> result;
    result.reserve(points_.size());
    for (const auto point : points_) {
        const auto element = circuit.at(point);
        if (element != Element::empty) result.push_back({point, element});
    }
    return result;
}
}
