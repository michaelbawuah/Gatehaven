#include "test.hpp"
#include "gatehaven/viewport.hpp"

using namespace gatehaven;

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
