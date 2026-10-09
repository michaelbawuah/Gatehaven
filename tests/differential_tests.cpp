#include "test.hpp"
#include "reference_simulation.hpp"
#include <random>
using namespace gatehaven;
TEST("simulation matches the slow traversal oracle across seeded mixed circuits and edits") {
    std::mt19937 random(20261009);
    for (unsigned trial = 0; trial < 60; ++trial) {
        Circuit circuit;
        for (Coordinate y = -4; y <= 4; ++y) for (Coordinate x = -6; x <= 6; ++x) {
            if (random() % 3 != 0) circuit.set({x, y}, static_cast<Element>(1 + random() % 13));
        }
        Simulation fast; reference::ReferenceSimulation slow;
        for (unsigned tick = 0; tick < 24; ++tick) {
            if (tick % 5 == 0) {
                const Point point{static_cast<Coordinate>(random() % 13) - 6, static_cast<Coordinate>(random() % 9) - 4};
                circuit.set(point, static_cast<Element>(random() % 14));
                const std::array changed{point}; fast.invalidate(changed); slow.invalidate(changed);
            }
            const Simulation::Exchange exchange = [tick](const CommunicatorGroup& group, bool sending) {
                return ((static_cast<std::uint32_t>(group.id.x) ^ tick) & 1U) != 0 && !sending;
            };
            fast.step(circuit, exchange); slow.step(circuit, exchange);
            CHECK(fast.snapshot() == slow.snapshot());
            CHECK(fast.powered_count() == slow.powered_count());
            for (const auto& cell : circuit.cells()) CHECK(fast.sent(cell.position) == slow.sent(cell.position));
        }
    }
}

TEST("optimized state survives reset invalidation copies and topology replacement") {
    Circuit circuit;
    circuit.set({0, 0}, Element::source); circuit.set({1, 0}, Element::wire);
    Simulation fast; reference::ReferenceSimulation slow;
    for (unsigned operation = 0; operation < 40; ++operation) {
        if (operation % 7 == 0) { fast.reset(); slow.reset(); }
        if (operation % 5 == 0) {
            const std::array changed{Point{1, 0}};
            fast.invalidate(changed); slow.invalidate(changed);
        }
        CHECK(fast.snapshot() == slow.snapshot());
        fast.step(circuit); slow.step(circuit);
        CHECK(fast.snapshot() == slow.snapshot());
        auto copy = fast; copy.reset();
        CHECK(copy.snapshot().empty()); CHECK(fast.snapshot() == slow.snapshot());
        if (operation == 20) { Circuit replacement; replacement.set({0, 0}, Element::nor_gate); circuit = std::move(replacement); }
    }
}

TEST("settled optimization wakes on edits and never skips communicator exchanges") {
    Circuit circuit; circuit.set({0, 0}, Element::source); circuit.set({1, 0}, Element::wire);
    Simulation simulation;
    for (unsigned i = 0; i < 100; ++i) simulation.step(circuit);
    CHECK(simulation.ticks() == 100); CHECK(simulation.metrics().topology_builds == 1);
    CHECK(simulation.metrics().propagations == 2); CHECK(simulation.metrics().settled_ticks == 98);
    circuit.set({0, 0}, Element::empty); simulation.step(circuit);
    CHECK(!simulation.powered({1, 0})); CHECK(simulation.metrics().topology_builds == 2);
    circuit.set({2, 0}, Element::screen);
    unsigned exchanges = 0;
    for (unsigned i = 0; i < 10; ++i) simulation.step(circuit, [&](const auto&, bool) { ++exchanges; return exchanges % 2 != 0; });
    CHECK(exchanges == 10); CHECK(!simulation.powered({2, 0}));
}

TEST("indexed adjacency matches reference at both coordinate extremes") {
    for (const auto edge : {std::numeric_limits<Coordinate>::min(), std::numeric_limits<Coordinate>::max() - 3}) {
        Circuit circuit;
        circuit.set({edge, edge}, Element::source);
        circuit.set({static_cast<Coordinate>(edge + 1), edge}, Element::crossing);
        circuit.set({static_cast<Coordinate>(edge + 2), edge}, Element::wire);
        circuit.set({static_cast<Coordinate>(edge + 2), static_cast<Coordinate>(edge + 1)}, Element::signal);
        circuit.set({static_cast<Coordinate>(edge + 3), static_cast<Coordinate>(edge + 1)}, Element::nor_gate);
        Simulation fast; reference::ReferenceSimulation slow;
        for (unsigned i = 0; i < 12; ++i) { fast.step(circuit); slow.step(circuit); CHECK(fast.snapshot() == slow.snapshot()); }
    }
}

TEST("copying a running feedback circuit keeps all engine workspaces independent") {
    Circuit circuit;
    circuit.set({0, 0}, Element::nor_gate); circuit.set({1, 0}, Element::wire);
    circuit.set({1, 1}, Element::wire); circuit.set({0, 1}, Element::signal);
    Simulation first; first.step(circuit); Simulation second = first;
    first.step(circuit); CHECK(!first.powered({1, 1})); CHECK(second.powered({1, 1}));
    const auto unchanged = second.snapshot(); first.step(circuit); first.reset();
    CHECK(second.snapshot() == unchanged); second.step(circuit); CHECK(!second.powered({1, 1}));
}
