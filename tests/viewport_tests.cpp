#include "test.hpp"
#include "gatehaven/viewport.hpp"

using namespace gatehaven;

TEST("centering a cell aligns its center with the viewport center at coordinate extremes") {
    Viewport view; const Point point{std::numeric_limits<Coordinate>::max(), std::numeric_limits<Coordinate>::min()};
    view.center_on(point);
    CHECK(view.cell(view.area.x + view.area.width / 2, view.area.y + view.area.height / 2) == point);
}

TEST("zoom preserves the world position underneath the cursor") {
    Viewport v;
    const auto before = v.world(811, 326);
    v.zoom(1.5, 811, 326);
    const auto after = v.world(811, 326);
    CHECK(std::abs(before.first - after.first) < 1e-9);
    CHECK(std::abs(before.second - after.second) < 1e-9);
}

TEST("negative fractional world positions select the correct grid cell") {
    Viewport v;
    v.center_x = -0.1;
    v.center_y = -1.1;
    CHECK(v.cell(v.area.x + v.area.width / 2, v.area.y + v.area.height / 2) == Point{-1, -2});
}

TEST("viewport arithmetic remains safe at world boundaries") {
    Viewport v;
    v.pan(1e20, -1e20);
    CHECK(!v.cell(-1e30, 0));
    const auto visible = v.visible();
    CHECK(visible.min.x <= visible.max.x);
    CHECK(visible.min.y <= visible.max.y);
    v.zoom(1e100, 500, 500);
    CHECK(v.scale == 80);
    v.zoom(1e-100, 500, 500);
    CHECK(v.scale == 4);
}

TEST("invalid zoom coordinates do not poison the camera") {
    Viewport view; const auto original_x = view.center_x; const auto original_y = view.center_y;
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    view.zoom(2, nan, 0); view.zoom(2, 0, std::numeric_limits<double>::infinity());
    CHECK(view.center_x == original_x && view.center_y == original_y && view.scale == 32);
    CHECK(!view.cell(nan, 0));
    view.center_x = nan;
    CHECK(view.visible().min.x == 0);
}
