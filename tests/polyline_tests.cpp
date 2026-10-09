#include "test.hpp"
#include "gatehaven/polyline.hpp"
using namespace gatehaven;

TEST("polyline snaps each new segment and remains one undoable edit") {
    Polyline line({0, 0}, Element::wire);
    CHECK(line.append({4, 1}).value());
    CHECK(line.append({5, 4}).value());
    CHECK(line.vertices().back() == Point{4, 4});
    Circuit circuit; History history;
    CHECK(history.apply(circuit, line.edits().value()).value());
    CHECK(circuit.size() == 9);
    CHECK(history.undo(circuit) && circuit.empty());
}
TEST("polyline preview and backtracking never alter the committed vertices") {
    Polyline line({-3, -2}, Element::source);
    CHECK(line.append({2, -2}));
    CHECK(line.preview({2, 5})->size() == 13);
    CHECK(line.vertices().size() == 2);
    CHECK(line.backtrack());
    CHECK(!line.backtrack());
    CHECK(line.edits()->size() == 1);
    CHECK(!line.append({-3, -2}).value());
}
TEST("polyline rejects excessive and diagonal segments without partial output") {
    const std::array diagonal{Point{0, 0}, Point{1, 1}};
    CHECK(!polyline_stroke(diagonal, Element::wire));
    const std::array long_line{Point{0, 0}, Point{100, 0}};
    CHECK(!polyline_stroke(long_line, Element::wire, 10));
    Polyline line({0, 0}, Element::wire);
    CHECK(!line.append({std::numeric_limits<Coordinate>::max(), 0}));
    CHECK(line.vertices().size() == 1);
}
TEST("insulated polyline turns conduct while straight portions stay insulated") {
    Polyline line({0, 0}, Element::crossing);
    CHECK(line.append({3, 0})); CHECK(line.append({3, 3}));
    Circuit c; History history; CHECK(history.apply(c, line.edits().value()));
    CHECK(c.at({3, 0}) == Element::wire);
    CHECK(c.at({2, 0}) == Element::crossing);
    CHECK(line.backtrack());
    const auto shortened = line.edits().value();
    CHECK(shortened.back().element == Element::crossing);
}
TEST("polyline resource bounds count shared vertices once and reject zero capacity") {
    const std::array points{Point{0, 0}, Point{2, 0}, Point{2, 2}};
    CHECK(polyline_stroke(points, Element::wire, 5)->size() == 5);
    CHECK(!polyline_stroke(points, Element::wire, 4));
    CHECK(!polyline_stroke(std::span(points).first(1), Element::wire, 0));
}

TEST("clipped polyline previews preserve crossing corners and retraced overlaps") {
    Polyline line({-100, 0}, Element::crossing);
    CHECK(line.append({0, 0})); CHECK(line.append({0, 100})); CHECK(line.append({3, 100}));
    const Bounds clip{{-2, -2}, {3, 3}};
    std::vector<Cell> expected;
    for (const auto cell : *line.preview({3, 0})) if (clip.contains(cell.position)) expected.push_back(cell);
    CHECK(line.preview({3, 0}, clip).value() == expected);
    CHECK(polyline_work(line.vertices()).value() == 204);
    CHECK(!polyline_work(line.vertices(), 203));
    CHECK(line.backtrack()); CHECK(line.preview({0, 0}, clip));
}

TEST("polyline validation rejects over-budget retracing before materializing cells") {
    const std::array vertices{Point{0, 0}, Point{999999, 0}, Point{0, 0}};
    CHECK(!polyline_work(vertices)); CHECK(!polyline_stroke(vertices, Element::wire));
    Polyline invalid({0, 0}, static_cast<Element>(255));
    CHECK(!invalid.preview({0, 0}, {{-1, -1}, {1, 1}}));
}
