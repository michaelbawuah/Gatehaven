#pragma once
#include "gatehaven/editor.hpp"

namespace gatehaven {
[[nodiscard]] std::expected<std::size_t, std::string> polyline_work(std::span<const Point> vertices, std::size_t limit = 1'000'000);
[[nodiscard]] Point snapped_endpoint(Point from, Point target);
[[nodiscard]] std::expected<std::vector<Cell>, std::string> polyline_stroke(
    std::span<const Point> vertices, Element element, std::size_t limit = 1'000'000);

class Polyline {
public:
    Polyline(Point start, Element element) : vertices_{start}, element_(element) {}
    [[nodiscard]] std::expected<bool, std::string> append(Point target);
    [[nodiscard]] std::expected<std::vector<Cell>, std::string> preview(Point target) const;
    [[nodiscard]] std::expected<std::vector<Cell>, std::string> edits() const;
    [[nodiscard]] bool backtrack();
    [[nodiscard]] std::span<const Point> vertices() const { return vertices_; }
private:
    std::vector<Point> vertices_;
    Element element_;
};
}
