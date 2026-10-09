#include "test.hpp"
#include "gatehaven/simulation.hpp"

#include <algorithm>
#include <limits>
#include <random>

using namespace gatehaven;

TEST("wire networks propagate in a single step and loops terminate") {
    Circuit c;
    c.set({0, 0}, Element::source);
    for (Coordinate x = 1; x <= 500; ++x) c.set({x, 0}, Element::wire);
    c.set({2, 1}, Element::wire);
    c.set({1, 1}, Element::wire);
    Simulation s;
    s.step(c);
    CHECK(s.powered({500, 0}));
    CHECK(s.powered_count() == c.size());
    c.set({0, 0}, Element::empty);
    s.step(c);
    CHECK(s.powered_count() == 0);
}

TEST("crossing channels cannot turn or short into each other") {
    Circuit c;
    c.set({-1, 0}, Element::source);
    c.set({0, 0}, Element::crossing);
    c.set({1, 0}, Element::wire);
    c.set({0, 1}, Element::wire);
    Simulation s;
    s.step(c);
    CHECK(s.ports({0, 0}) == 10);
    CHECK(s.powered({1, 0}));
    CHECK(!s.powered({0, 1}));
    c.set({0, -1}, Element::source);
    s.step(c);
    CHECK(s.ports({0, 0}) == 15);
    CHECK(s.powered({0, 1}));
}

TEST("signals isolate adjacent gate outputs and sources") {
    Circuit c;
    c.set({0, 0}, Element::source);
    c.set({1, 0}, Element::signal);
    c.set({2, 0}, Element::and_gate);
    Simulation s;
    s.step(c);
    CHECK(!s.powered({1, 0}));
    CHECK(!s.powered({2, 0}));
    c.set({0, 1}, Element::wire);
    c.set({1, 1}, Element::wire);
    s.step(c);
    CHECK(s.powered({1, 0}));
    CHECK(!s.powered({2, 0}));
    s.step(c);
    CHECK(s.powered({2, 0}));
}

TEST("gates read inputs one step later including falling edges") {
    Circuit c;
    c.set({-2, 0}, Element::source);
    c.set({-1, 0}, Element::wire);
    c.set({0, 0}, Element::signal);
    c.set({1, 0}, Element::or_gate);
    c.set({2, 0}, Element::wire);
    Simulation s;
    s.step(c);
    CHECK(!s.powered({2, 0}));
    s.step(c);
    CHECK(s.powered({2, 0}));
    c.set({-2, 0}, Element::empty);
    s.step(c);
    CHECK(s.powered({2, 0}));
    s.step(c);
    CHECK(!s.powered({2, 0}));
}

TEST("gate truth tables hold for zero through four adjacent signals") {
    const std::array gates{Element::and_gate, Element::or_gate, Element::nand_gate, Element::nor_gate};
    for (const auto gate : gates) {
        for (unsigned inputs = 0; inputs <= 4; ++inputs) {
            for (unsigned powered = 0; powered <= inputs; ++powered) {
                Circuit c;
                c.set({0, 0}, gate);
                for (unsigned i = 0; i < inputs; ++i) {
                    const auto input = *neighbor({0, 0}, directions[i]);
                    c.set(input, Element::signal);
                    const auto wire = *neighbor(input, directions[i]);
                    c.set(wire, Element::wire);
                    if (i < powered) c.set(*neighbor(wire, directions[i]), Element::source);
                }
                Simulation s;
                s.step(c);
                s.step(c);
                bool expected = false;
                if (gate == Element::and_gate) expected = powered == inputs;
                if (gate == Element::or_gate) expected = powered != 0;
                if (gate == Element::nand_gate) expected = powered != inputs;
                if (gate == Element::nor_gate) expected = powered == 0;
                CHECK(s.powered({0, 0}) == expected);
            }
        }
    }
}

TEST("relays conduct without generating power") {
    for (const auto relay : {Element::positive_relay, Element::negative_relay}) {
        for (const bool input_on : {false, true}) {
            Circuit c;
            c.set({0, 0}, relay);
            c.set({0, -1}, Element::signal);
            c.set({0, -2}, Element::wire);
            if (input_on) c.set({0, -3}, Element::source);
            c.set({1, 0}, Element::wire);
            Simulation s;
            s.step(c);
            s.step(c);
            CHECK(!s.powered({1, 0}));
            c.set({-1, 0}, Element::source);
            s.step(c);
            CHECK(s.powered({1, 0}) == (relay == Element::positive_relay ? input_on : !input_on));
        }
    }
}

TEST("relays without signals remain nonconductive") {
    for (const auto relay : {Element::positive_relay, Element::negative_relay}) {
        Circuit c;
        c.set({0, 0}, Element::source);
        c.set({1, 0}, relay);
        c.set({2, 0}, Element::wire);
        Simulation s;
        s.step(c);
        CHECK(!s.powered({2, 0}));
    }
}

TEST("reset clears state and edits remove stale powered cells") {
    Circuit c;
    c.set({0, 0}, Element::source);
    Simulation s;
    s.step(c);
    CHECK(s.ticks() == 1);
    c.clear();
    s.step(c);
    CHECK(s.snapshot().empty());
    s.reset();
    CHECK(s.ticks() == 0);
}

TEST("random insertion orders produce identical states across many steps") {
    std::mt19937 random(941);
    Circuit first;
    for (Coordinate y = -10; y < 10; ++y) {
        for (Coordinate x = -10; x < 10; ++x) {
            first.set({x, y}, static_cast<Element>(random() % element_names.size()));
        }
    }
    auto cells = first.cells();
    std::shuffle(cells.begin(), cells.end(), random);
    Circuit second;
    for (const auto& cell : cells) second.set(cell.position, cell.element);
    Simulation a;
    Simulation b;
    for (unsigned i = 0; i < 40; ++i) {
        a.step(first);
        b.step(second);
        CHECK(a.snapshot() == b.snapshot());
    }
}

TEST("simulation does not connect across integer coordinate boundaries") {
    Circuit c;
    c.set({std::numeric_limits<Coordinate>::max(), 0}, Element::source);
    c.set({std::numeric_limits<Coordinate>::min(), 0}, Element::wire);
    Simulation s;
    s.step(c);
    CHECK(s.powered_count() == 1);
}
