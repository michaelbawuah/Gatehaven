#include "test.hpp"
#include "gatehaven/editor.hpp"

#include <limits>

using namespace gatehaven;

TEST("saved history checkpoints survive undo redo and divergent branches") {
    Circuit c; History h;
    const std::array a{Cell{{0, 0}, Element::wire}}, b{Cell{{1, 0}, Element::source}};
    CHECK(!h.modified()); CHECK(h.apply(c, a)); h.mark_saved(); CHECK(!h.modified());
    CHECK(h.apply(c, b)); CHECK(h.modified()); CHECK(h.undo(c)); CHECK(!h.modified());
    CHECK(h.redo(c)); CHECK(h.modified()); CHECK(h.undo(c)); CHECK(h.undo(c)); CHECK(h.modified());
    CHECK(h.apply(c, b)); CHECK(h.modified()); CHECK(!h.can_redo());
    h.mark_saved(); CHECK(!h.modified()); CHECK(h.undo_depth() == 1 && h.redo_depth() == 0);
}

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

TEST("selection move handles overlap and restores overwritten destination cells") {
    Circuit c;
    c.set({0, 0}, Element::source);
    c.set({1, 0}, Element::wire);
    c.set({2, 0}, Element::nor_gate);
    const auto original = c;
    const auto moved = move_region(c, {{0, 0}, {1, 0}}, 1, 0);
    CHECK(moved && moved->region.min == Point{1, 0} && moved->region.max == Point{2, 0});
    History history;
    CHECK(history.apply(c, moved->edits).value());
    CHECK(c.at({0, 0}) == Element::empty);
    CHECK(c.at({1, 0}) == Element::source);
    CHECK(c.at({2, 0}) == Element::wire);
    CHECK(history.undo(c));
    CHECK(c == original);
    CHECK(history.redo(c));
    CHECK(c.at({2, 0}) == Element::wire);
}

TEST("moving a selection validates empty borders and extreme offsets before edits") {
    Circuit c;
    constexpr auto hi = std::numeric_limits<Coordinate>::max();
    const Bounds region{{hi - 3, 0}, {hi, 1}};
    c.set(region.min, Element::source);
    CHECK(!move_region(c, region, 1, 0));
    CHECK(!move_region(c, region, std::numeric_limits<std::int64_t>::max(), 0));
    CHECK(!move_region(c, region, 0, std::numeric_limits<std::int64_t>::min()));
    CHECK(!move_region(c, {{2, 0}, {1, 0}}, 0, 0));
    const auto moved = move_region(c, {{0, 0}, {4, 4}}, -4, 4);
    CHECK(moved && moved->edits.empty() && moved->region.min == Point{-4, 4});
    CHECK(c.size() == 1);
}
TEST("paste validates empty borders and malformed stamp cells before returning edits") {
    const auto hi = std::numeric_limits<Coordinate>::max();
    CHECK(!paste({2, 1, {{0, 0, Element::source}}}, {hi, 0}));
    CHECK(!paste({1, 1, {{-1, 0, Element::wire}}}, {0, 0}));
    CHECK(!paste({1, 1, {{0, 0, Element::empty}}}, {0, 0}));
    CHECK(!paste({0, 1, {}}, {0, 0}));
    CHECK(paste({}, {0, 0})->empty());
}

TEST("recovered document baselines remain unsaved after undoing their first edit") {
    Circuit circuit; circuit.set({0, 0}, Element::wire); History history;
    history.mark_unsaved(); CHECK(history.modified()); CHECK(!history.can_undo());
    const std::array edit{Cell{{1, 0}, Element::source}};
    CHECK(history.apply(circuit, edit)); CHECK(history.undo(circuit)); CHECK(history.modified());
    history.mark_saved(); CHECK(!history.modified());
}

TEST("pencil rejects invalid tools even for a single cell") {
    CHECK(!pencil_line({0, 0}, {0, 0}, static_cast<Element>(255)));
    CHECK(!pencil_line({0, 0}, {0, 0}, Element::wire, 0));
    CHECK(pencil_line({0, 0}, {0, 0}, Element::empty, 1)->size() == 1);
}

TEST("history keeps last-write semantics across interleaved duplicate coordinates") {
    Circuit c; History h;
    const std::array edits{Cell{{2, 0}, Element::source}, Cell{{1, 0}, Element::wire},
        Cell{{2, 0}, Element::empty}, Cell{{1, 0}, Element::nor_gate}, Cell{{-1, 0}, Element::signal}};
    CHECK(h.apply(c, edits).value()); CHECK(c.size() == 2);
    CHECK(c.at({1, 0}) == Element::nor_gate); CHECK(c.at({2, 0}) == Element::empty);
    CHECK(h.last_changes().size() == 2); CHECK(h.undo(c)); CHECK(c.empty());
    CHECK(h.redo(c)); CHECK(c.at({1, 0}) == Element::nor_gate);
}
