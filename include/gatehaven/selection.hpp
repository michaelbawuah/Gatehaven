#pragma once
#include "gatehaven/editor.hpp"
#include <set>

namespace gatehaven {
enum class SelectionMode { replace, add, subtract };
class Selection {
public:
    static Selection rectangle(const Circuit& circuit, Bounds region);
    static Selection points(std::set<Point> points);
    void combine(const Selection& other, SelectionMode mode);
    void clear() { points_.clear(); frame_.reset(); }
    [[nodiscard]] bool contains(Point p) const { return points_.contains(p); }
    [[nodiscard]] const std::set<Point>& points() const { return points_; }
    [[nodiscard]] std::optional<Bounds> bounds() const { return frame_; }
    [[nodiscard]] explicit operator bool() const { return frame_.has_value(); }
    [[nodiscard]] std::size_t size() const { return points_.size(); }
    [[nodiscard]] std::vector<Cell> cells(const Circuit& circuit) const;
    void set_frame(Bounds region) { frame_ = region; }
private:
    std::set<Point> points_;
    std::optional<Bounds> frame_;
};
[[nodiscard]] Selection connected_selection(const Circuit& circuit, Point seed, bool physical);
[[nodiscard]] Stamp capture_selection(const Circuit& circuit, const Selection& selection);
struct ShapeEdit { Selection selection; std::vector<Cell> edits; };
[[nodiscard]] std::expected<ShapeEdit, std::string> place_selection(
    const Circuit& circuit, const Selection& selection, const Stamp& stamp, Point origin);
[[nodiscard]] std::expected<ShapeEdit, std::string> move_selection(
    const Circuit& circuit, const Selection& selection, std::int64_t dx, std::int64_t dy);
}
