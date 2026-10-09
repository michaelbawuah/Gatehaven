#include "test.hpp"
#include "gatehaven/selection.hpp"
using namespace gatehaven;

TEST("selection combines sparse rectangles without selecting cells between them") {
    Circuit c;
    c.set({0, 0}, Element::wire); c.set({1, 0}, Element::source); c.set({2, 0}, Element::wire);
    auto selected = Selection::rectangle(c, {{0, 0}, {0, 0}});
    selected.combine(Selection::rectangle(c, {{2, 0}, {2, 0}}), SelectionMode::add);
    CHECK(selected.size() == 2 && !selected.contains({1, 0}));
    selected.combine(Selection::rectangle(c, {{0, 0}, {0, 0}}), SelectionMode::subtract);
    CHECK(selected.size() == 1 && selected.bounds()->min == Point{2, 0});
    selected.combine(Selection::rectangle(c, {{2, 0}, {2, 0}}), SelectionMode::subtract);
    CHECK(!selected);
}
TEST("empty rectangle borders are retained without allocating empty cells") {
    Circuit c;
    const Bounds region{{-1000000, -1000000}, {1000000, 1000000}};
    const auto selected = Selection::rectangle(c, region);
    CHECK(selected && selected.size() == 0);
    CHECK(selected.bounds()->max == region.max);
    CHECK(!Selection::rectangle(c, {{2, 0}, {1, 0}}));
}
