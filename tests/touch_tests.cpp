#include "test.hpp"
#include "gatehaven/touch.hpp"
#include <cmath>
#include <limits>
using namespace gatehaven;
TEST("touch strokes keep ownership until release and suppress draw after a pinch") {
    TouchGesture touch;
    CHECK(touch.down({1, 1}, {10, 20}).action == TouchAction::begin);
    CHECK(touch.move({2, 1}, {15, 20}).action == TouchAction::none);
    CHECK(touch.move({1, 1}, {15, 20}).action == TouchAction::move);
    CHECK(touch.down({1, 2}, {25, 20}).action == TouchAction::cancel);
    CHECK(touch.up({1, 2}, {25, 20}).action == TouchAction::none);
    CHECK(touch.move({1, 1}, {16, 20}).action == TouchAction::none);
    CHECK(touch.up({1, 1}, {16, 20}).action == TouchAction::none);
    CHECK(touch.down({1, 3}, {0, 0}).action == TouchAction::begin);
    CHECK(touch.up({1, 3}, {1, 0}).action == TouchAction::end);
}
TEST("pinch computes exact scale and centroid while extra contacts suspend navigation") {
    TouchGesture touch;
    static_cast<void>(touch.down({1, 1}, {0, 0})); static_cast<void>(touch.down({1, 2}, {10, 0}));
    const auto pinch = touch.move({1, 2}, {20, 0});
    CHECK(pinch.action == TouchAction::navigate); CHECK(pinch.zoom == 2);
    CHECK(pinch.before.x == 5 && pinch.after.x == 10);
    static_cast<void>(touch.down({1, 3}, {40, 0}));
    CHECK(touch.move({1, 1}, {5, 0}).action == TouchAction::none);
    static_cast<void>(touch.up({1, 3}, {40, 0}));
    CHECK(touch.move({1, 1}, {6, 0}).action == TouchAction::navigate);
    touch.clear(); CHECK(touch.size() == 0);
    CHECK(touch.down({1, 1}, {std::numeric_limits<double>::quiet_NaN(), 0}).action == TouchAction::none);
}
