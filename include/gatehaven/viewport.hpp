#pragma once

#include "gatehaven/types.hpp"

#include <algorithm>
#include <cmath>

namespace gatehaven {

struct ViewRect {
    double x{}, y{}, width{}, height{};
    [[nodiscard]] bool contains(double px, double py) const {
        return px >= x && py >= y && px < x + width && py < y + height;
    }
};

class Viewport {
public:
    ViewRect area{240, 88, 1040, 676};
    double center_x{0.5};
    double center_y{3.5};
    double scale{32};

    [[nodiscard]] std::pair<double, double> world(double x, double y) const {
        return {center_x + (x - area.x - area.width / 2) / scale,
                center_y + (y - area.y - area.height / 2) / scale};
    }

    [[nodiscard]] std::pair<double, double> screen(Point p) const {
        return {area.x + area.width / 2 + (static_cast<double>(p.x) - center_x) * scale,
                area.y + area.height / 2 + (static_cast<double>(p.y) - center_y) * scale};
    }

    [[nodiscard]] std::optional<Point> cell(double x, double y) const {
        const auto [wx, wy] = world(x, y);
        const auto fx = std::floor(wx);
        const auto fy = std::floor(wy);
        if (!std::isfinite(fx) || !std::isfinite(fy) || fx < lo || fx > hi || fy < lo || fy > hi) {
            return std::nullopt;
        }
        return Point{static_cast<Coordinate>(fx), static_cast<Coordinate>(fy)};
    }

    [[nodiscard]] Bounds visible() const {
        const auto [left, top] = world(area.x, area.y);
        const auto [right, bottom] = world(area.x + area.width, area.y + area.height);
        return {{bounded(std::floor(left)), bounded(std::floor(top))},
                {bounded(std::ceil(right)), bounded(std::ceil(bottom))}};
    }

    void pan(double dx, double dy) {
        if (!std::isfinite(dx) || !std::isfinite(dy)) return;
        center_x = std::clamp(center_x - dx / scale, lo, hi);
        center_y = std::clamp(center_y - dy / scale, lo, hi);
    }

    void center_on(Point point) { center_x = static_cast<double>(point.x) + 0.5; center_y = static_cast<double>(point.y) + 0.5; }

    void zoom(double multiplier, double x, double y) {
        if (!std::isfinite(multiplier) || multiplier <= 0) return;
        const auto before = world(x, y);
        scale = std::clamp(scale * multiplier, 4.0, 80.0);
        const auto after = world(x, y);
        center_x = std::clamp(center_x + before.first - after.first, lo, hi);
        center_y = std::clamp(center_y + before.second - after.second, lo, hi);
    }

    void frame(std::optional<Bounds> bounds) {
        if (!bounds) { center_x = center_y = 0; scale = 32; return; }
        center_x = (static_cast<double>(bounds->min.x) + bounds->max.x + 1) / 2;
        center_y = (static_cast<double>(bounds->min.y) + bounds->max.y + 1) / 2;
        const double width = static_cast<double>(bounds->max.x) - bounds->min.x + 5;
        const double height = static_cast<double>(bounds->max.y) - bounds->min.y + 5;
        scale = std::clamp(std::min({area.width / width, area.height / height, 40.0}), 4.0, 80.0);
    }

private:
    static constexpr double lo = std::numeric_limits<Coordinate>::min();
    static constexpr double hi = std::numeric_limits<Coordinate>::max();
    static Coordinate bounded(double value) {
        return static_cast<Coordinate>(std::clamp(value, lo, hi));
    }
};

} // namespace gatehaven
