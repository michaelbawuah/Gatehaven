#include "gatehaven/polyline.hpp"
#include <cstdlib>
#include <map>

namespace gatehaven {
Point snapped_endpoint(Point from, Point target) {
    const auto dx = static_cast<std::int64_t>(target.x) - from.x;
    const auto dy = static_cast<std::int64_t>(target.y) - from.y;
    return std::abs(dx) >= std::abs(dy) ? Point{target.x, from.y} : Point{from.x, target.y};
}

std::expected<std::size_t, std::string> polyline_work(std::span<const Point> vertices, std::size_t limit) {
    if (vertices.empty()) return 0;
    if (vertices.size() > 65536 || limit == 0) return std::unexpected("Polyline exceeds edit limit");
    std::size_t work = 1;
    for (std::size_t i = 1; i < vertices.size(); ++i) {
        const auto dx = static_cast<std::int64_t>(vertices[i].x) - vertices[i - 1].x;
        const auto dy = static_cast<std::int64_t>(vertices[i].y) - vertices[i - 1].y;
        if (dx != 0 && dy != 0) return std::unexpected("Polyline segment must follow a grid axis");
        const auto distance = static_cast<std::uint64_t>(std::abs(dx) + std::abs(dy));
        if (distance > limit - work) return std::unexpected("Polyline exceeds edit limit");
        work += static_cast<std::size_t>(distance);
    }
    return work;
}

std::expected<std::vector<Cell>, std::string> polyline_stroke(
    std::span<const Point> vertices, Element element, std::size_t limit) {
    if (vertices.empty()) return std::vector<Cell>{};
    const auto budget = polyline_work(vertices, limit);
    if (!budget) return std::unexpected(budget.error());
    if (vertices.size() > 65536 || static_cast<std::size_t>(element) >= element_names.size()) {
        return std::unexpected("Invalid polyline size or element");
    }
    std::map<Point, Element> cells;
    cells[vertices.front()] = element;
    for (std::size_t i = 1; i < vertices.size(); ++i) {
        if (vertices[i - 1].x != vertices[i].x && vertices[i - 1].y != vertices[i].y) {
            return std::unexpected("Polyline segment must follow a grid axis");
        }
        const auto segment = pencil_line(vertices[i - 1], vertices[i], element, limit);
        if (!segment) return std::unexpected(segment.error());
        for (const auto& cell : *segment) cells[cell.position] = cell.element;
    }
    if (element == Element::crossing) {
        for (std::size_t i = 1; i + 1 < vertices.size(); ++i) {
            const bool incoming_horizontal = vertices[i - 1].y == vertices[i].y;
            const bool outgoing_horizontal = vertices[i].y == vertices[i + 1].y;
            if (incoming_horizontal != outgoing_horizontal) cells[vertices[i]] = Element::wire;
        }
    }
    std::vector<Cell> edits;
    edits.reserve(cells.size());
    for (const auto& [point, value] : cells) edits.push_back({point, value});
    return edits;
}

std::expected<bool, std::string> Polyline::append(Point target) {
    const auto next = snapped_endpoint(vertices_.back(), target);
    if (next == vertices_.back()) return false;
    auto candidate = vertices_;
    candidate.push_back(next);
    if (static_cast<std::size_t>(element_) >= element_names.size()) return std::unexpected("Invalid polyline element");
    const auto result = polyline_work(candidate);
    if (!result) return std::unexpected(result.error());
    vertices_ = std::move(candidate);
    return true;
}
std::expected<std::vector<Cell>, std::string> Polyline::preview(Point target) const {
    auto candidate = vertices_;
    const auto next = snapped_endpoint(candidate.back(), target);
    if (next != candidate.back()) candidate.push_back(next);
    return polyline_stroke(candidate, element_);
}
std::expected<std::vector<Cell>, std::string> Polyline::preview(Point target, Bounds clip) const {
    if (static_cast<std::size_t>(element_) >= element_names.size()) return std::unexpected("Invalid polyline element");
    auto vertices = vertices_;
    const auto next = snapped_endpoint(vertices.back(), target);
    if (next != vertices.back()) vertices.push_back(next);
    const auto work = polyline_work(vertices);
    if (!work) return std::unexpected(work.error());
    std::map<Point, Element> visible;
    if (clip.contains(vertices.front())) visible[vertices.front()] = element_;
    for (std::size_t i = 1; i < vertices.size(); ++i) {
        const auto segment = clipped_pencil_line(vertices[i - 1], vertices[i], element_, clip);
        if (!segment) return std::unexpected(segment.error());
        for (const auto cell : *segment) visible[cell.position] = cell.element;
    }
    if (element_ == Element::crossing) for (std::size_t i = 1; i + 1 < vertices.size(); ++i) {
        if (clip.contains(vertices[i]) && ((vertices[i - 1].y == vertices[i].y) != (vertices[i].y == vertices[i + 1].y)))
            visible[vertices[i]] = Element::wire;
    }
    std::vector<Cell> result; result.reserve(visible.size());
    for (const auto& [point, element] : visible) result.push_back({point, element});
    return result;
}
std::expected<std::vector<Cell>, std::string> Polyline::edits() const { return polyline_stroke(vertices_, element_); }
bool Polyline::backtrack() {
    if (vertices_.size() < 2) return false;
    vertices_.pop_back(); return true;
}
}
