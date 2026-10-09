#include "test.hpp"
#include "gatehaven/circuit.hpp"

#include <limits>

using namespace gatehaven;

TEST("communicator element identifiers round trip by name") {
    for (const auto e : {Element::screen, Element::file_input, Element::file_output}) {
        CHECK(is_communicator(e)); CHECK(parse_element(name(e)).value() == e);
    }
    CHECK(!is_communicator(Element::signal));
}
TEST("element counts remain exact through replacements no-ops copies and clear") {
    Circuit c;
    c.set({0, 0}, Element::wire); c.set({1, 0}, Element::wire); c.set({1, 0}, Element::wire);
    CHECK(c.count(Element::wire) == 2);
    c.set({1, 0}, Element::screen); CHECK(c.count(Element::wire) == 1 && c.count(Element::screen) == 1);
    const auto copied = c; CHECK(copied.count(Element::screen) == 1);
    c.set({1, 0}, Element::empty); CHECK(c.count(Element::screen) == 0);
    c.clear(); CHECK(c.count(Element::wire) == 0 && c.count(Element::empty) == 0);
}

TEST("sparse cells support negative and extreme coordinates") {
    Circuit circuit;
    circuit.set({-8, 4}, Element::wire);
    circuit.set({std::numeric_limits<Coordinate>::max(), -100}, Element::source);
    CHECK(circuit.size() == 2);
    CHECK(circuit.at({0, 0}) == Element::empty);
    CHECK(circuit.at({-8, 4}) == Element::wire);
    CHECK(circuit.set({-8, 4}, Element::empty));
    CHECK(!circuit.set({-8, 4}, Element::empty));
    CHECK(circuit.size() == 1);
}

TEST("visible queries include boundaries and ignore distant cells") {
    Circuit circuit;
    circuit.set({-4, -4}, Element::wire);
    circuit.set({4, 4}, Element::source);
    circuit.set({1000000000, 0}, Element::wire);
    CHECK(circuit.cells_in({{-4, -4}, {4, 4}}).size() == 2);
    CHECK(circuit.cells_in({{4, 4}, {-4, -4}}).empty());
    CHECK(circuit.bounds()->max.x == 1000000000);
}

TEST("coordinate arithmetic cannot wrap the world") {
    constexpr auto hi = std::numeric_limits<Coordinate>::max();
    constexpr auto lo = std::numeric_limits<Coordinate>::min();
    CHECK(!neighbor({hi, 0}, Direction::east));
    CHECK(!neighbor({0, lo}, Direction::north));
    CHECK(!translated({0, 0}, std::numeric_limits<std::int64_t>::max(), 0));
    CHECK(translated({hi, lo}, -1, 1) == Point{hi - 1, lo + 1});
}

TEST("enumeration order is independent of insertion order") {
    Circuit first;
    Circuit second;
    first.set({2, 4}, Element::wire);
    first.set({-9, -4}, Element::source);
    second.set({-9, -4}, Element::source);
    second.set({2, 4}, Element::wire);
    CHECK(first.cells() == second.cells());
    CHECK(first == second);
    second.clear();
    CHECK(second.empty());
    CHECK(!second.bounds());
}
