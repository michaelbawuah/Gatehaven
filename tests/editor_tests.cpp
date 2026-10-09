#include "test.hpp"
#include "gatehaven/editor.hpp"

#include <limits>

using namespace gatehaven;

TEST("drawing snaps to dominant axis and includes both endpoints") {
    const auto horizontal = pencil_line({0, 0}, {4, 2}, Element::wire);
    CHECK(horizontal && horizontal->size() == 5);
    CHECK(horizontal->back().position == Point{4, 0});
    const auto vertical = pencil_line({1, 1}, {0, -2}, Element::wire);
    CHECK(vertical && vertical->size() == 4);
    CHECK(vertical->back().position == Point{1, -2});
    CHECK(!pencil_line({0, 0}, {100, 0}, Element::wire, 10));
}

TEST("each stroke is undone and redone as a single transaction") {
    Circuit c;
    History history;
    const auto stroke = *pencil_line({-2, 0}, {2, 0}, Element::wire);
    CHECK(history.apply(c, stroke).value());
    CHECK(c.size() == 5);
    CHECK(history.undo(c));
    CHECK(c.empty());
    CHECK(history.redo(c));
    CHECK(c.size() == 5);
    CHECK(history.last_changes().size() == 5);
}

TEST("overlapping edits restore original values and use the final requested value") {
    Circuit c;
    c.set({0, 0}, Element::source);
    History history;
    const std::array edits{Cell{{0, 0}, Element::wire}, Cell{{0, 0}, Element::and_gate}};
    CHECK(history.apply(c, edits).value());
    CHECK(c.at({0, 0}) == Element::and_gate);
    CHECK(history.undo(c));
    CHECK(c.at({0, 0}) == Element::source);
}

TEST("a new edit clears redo but a no-op preserves it") {
    Circuit c;
    History history;
    const std::array edit{Cell{{0, 0}, Element::source}};
    CHECK(history.apply(c, edit));
    CHECK(history.undo(c));
    const std::array noop{Cell{{0, 0}, Element::empty}};
    CHECK(!history.apply(c, noop).value());
    CHECK(history.can_redo());
    const std::array other{Cell{{1, 0}, Element::wire}};
    CHECK(history.apply(c, other));
    CHECK(!history.can_redo());
}

TEST("oversized and invalid edits leave the entire document untouched") {
    Circuit c;
    History history(1);
    const std::array edits{Cell{{0, 0}, Element::wire}, Cell{{1, 0}, Element::source}};
    CHECK(!history.apply(c, edits));
    CHECK(c.empty());
    const std::array invalid{Cell{{0, 0}, Element::wire}, Cell{{1, 0}, static_cast<Element>(200)}};
    CHECK(!history.apply(c, invalid));
    CHECK(c.empty());
}

TEST("history memory budget retains the newest complete commands") {
    Circuit c;
    History history(2);
    for (Coordinate i = 0; i < 3; ++i) {
        const std::array edit{Cell{{i, 0}, Element::wire}};
        CHECK(history.apply(c, edit));
    }
    CHECK(history.undo(c));
    CHECK(history.undo(c));
    CHECK(!history.undo(c));
    CHECK(c.size() == 1);
}

TEST("clipboard rotations and flips preserve sparse geometry") {
    Circuit c;
    c.set({10, 20}, Element::source);
    c.set({12, 21}, Element::and_gate);
    auto stamp = capture(c, {{10, 20}, {12, 21}});
    stamp.rotate_clockwise();
    CHECK(stamp.width == 2 && stamp.height == 3);
    auto edits = paste(stamp, {0, 0});
    CHECK(edits && edits->front().position == Point{1, 0});
    CHECK(edits->back().position == Point{0, 2});
    stamp.rotate_clockwise();
    stamp.rotate_clockwise();
    stamp.rotate_clockwise();
    stamp.flip_horizontal();
    stamp.flip_horizontal();
    stamp.flip_vertical();
    stamp.flip_vertical();
    CHECK(paste(stamp, {10, 20}).value() == c.cells());
}

TEST("clipboard supports wide coordinates and rejects overflowing paste atomically") {
    Circuit c;
    constexpr auto lo = std::numeric_limits<Coordinate>::min();
    constexpr auto hi = std::numeric_limits<Coordinate>::max();
    c.set({lo, 0}, Element::source);
    c.set({hi, 0}, Element::wire);
    const auto stamp = capture(c, {{lo, 0}, {hi, 0}});
    CHECK(stamp.width == 4294967296LL);
    CHECK(paste(stamp, {lo, 0}));
    CHECK(!paste(stamp, {0, 0}));
}
