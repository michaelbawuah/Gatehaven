#include "gatehaven/selection.hpp"
#include <algorithm>
#include <map>
#include <utility>

namespace gatehaven {
Selection Selection::rectangle(const Circuit& circuit, Bounds region) {
    Selection result;
    if (region.min.x > region.max.x || region.min.y > region.max.y) return result;
    result.frame_ = region;
    circuit.visit(region, [&](const Cell& cell) { result.points_.insert(cell.position); });
    return result;
}

Selection connected_selection(const Circuit& circuit, Point seed, bool physical) {
    if (circuit.at(seed) == Element::empty) return {};
    std::map<Point, std::uint8_t> visited{{seed, std::uint8_t{15}}};
    std::vector<std::pair<Point, std::uint8_t>> queue{{seed, std::uint8_t{15}}};
    const auto wire = [](Element e) { return e == Element::wire || e == Element::crossing || e == Element::signal; };
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const auto [point, ports] = queue[head];
        const auto from = circuit.at(point);
        for (const auto direction : directions) {
            const auto bit = static_cast<std::uint8_t>(1U << std::to_underlying(direction));
            if (!physical && (ports & bit) == 0) continue;
            const auto next = neighbor(point, direction);
            if (!next) continue;
            const auto to = circuit.at(*next);
            if (to == Element::empty) continue;
            if (!physical && ((from == Element::signal && !wire(to)) || (to == Element::signal && !wire(from)))) continue;
            const auto channel = !physical && to == Element::crossing
                ? static_cast<std::uint8_t>((direction == Direction::north || direction == Direction::south) ? 5 : 10)
                : std::uint8_t{15};
            auto& seen = visited[*next];
            const auto added = static_cast<std::uint8_t>(channel & static_cast<std::uint8_t>(~seen));
            if (!added) continue;
            seen = static_cast<std::uint8_t>(seen | added);
            queue.emplace_back(*next, added);
        }
    }
    std::set<Point> points;
    for (const auto& [point, ports] : visited) { static_cast<void>(ports); points.insert(point); }
    return Selection::points(std::move(points));
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

Stamp capture_selection(const Circuit& circuit, const Selection& selection) {
    const auto region = selection.bounds();
    if (!region) return {};
    Stamp stamp{static_cast<std::int64_t>(region->max.x) - region->min.x + 1,
                static_cast<std::int64_t>(region->max.y) - region->min.y + 1, {}};
    for (const auto& cell : selection.cells(circuit)) {
        stamp.cells.push_back({static_cast<std::int64_t>(cell.position.x) - region->min.x,
                               static_cast<std::int64_t>(cell.position.y) - region->min.y, cell.element});
    }
    return stamp;
}

std::expected<ShapeEdit, std::string> place_selection(const Circuit& circuit, const Selection& selection,
                                                    const Stamp& stamp, Point origin) {
    if (stamp.width <= 0 || stamp.height <= 0) return std::unexpected("Selection is empty");
    const auto corner = translated(origin, stamp.width - 1, stamp.height - 1);
    const auto target = paste(stamp, origin);
    if (!corner || !target) return std::unexpected("Selection exceeds coordinate limits");
    ShapeEdit result;
    result.edits = selection.cells(circuit);
    for (auto& cell : result.edits) cell.element = Element::empty;
    result.edits.insert(result.edits.end(), target->begin(), target->end());
    std::set<Point> selected;
    for (const auto& cell : *target) selected.insert(cell.position);
    result.selection = Selection::points(std::move(selected));
    result.selection.set_frame({origin, *corner});
    return result;
}

std::expected<ShapeEdit, std::string> move_selection(const Circuit& circuit, const Selection& selection,
                                                   std::int64_t dx, std::int64_t dy) {
    if (!selection) return std::unexpected("Select a region first");
    const auto origin = translated(selection.bounds()->min, dx, dy);
    if (!origin) return std::unexpected("Selection exceeds coordinate limits");
    return place_selection(circuit, selection, capture_selection(circuit, selection), *origin);
}
}
