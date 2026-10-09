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
TEST("logical selection respects crossing channels and signal input boundaries") {
    Circuit c;
    c.set({-1, 0}, Element::wire); c.set({0, 0}, Element::crossing);
    c.set({1, 0}, Element::signal); c.set({2, 0}, Element::and_gate);
    c.set({0, 1}, Element::wire);
    const auto logical = connected_selection(c, {-1, 0}, false);
    CHECK(logical.size() == 3 && !logical.contains({0, 1}) && !logical.contains({2, 0}));
    CHECK(connected_selection(c, {-1, 0}, true).size() == 5);
    CHECK(!connected_selection(c, {5, 5}, true));
}
TEST("moving a sparse selection leaves holes and restores destination overwrites") {
    Circuit c;
    c.set({0, 0}, Element::wire); c.set({1, 0}, Element::source); c.set({2, 0}, Element::wire);
    const auto original = c;
    const auto selection = Selection::points({{0, 0}, {2, 0}});
    const auto move = move_selection(c, selection, 0, 1);
    CHECK(move && move->selection.size() == 2);
    History history; CHECK(history.apply(c, move->edits));
    CHECK(c.at({1, 0}) == Element::source && c.at({1, 1}) == Element::empty);
    CHECK(history.undo(c) && c == original);
    auto stamp = capture_selection(c, selection); stamp.rotate_clockwise();
    const auto turn = place_selection(c, selection, stamp, {0, 0});
    CHECK(turn && turn->selection.contains({0, 2}));
    CHECK(history.apply(c, turn->edits));
    CHECK(c.at({1, 0}) == Element::source);
}
