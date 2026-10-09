#include "test.hpp"
#include "gatehaven/examples.hpp"
#include "gatehaven/simulation.hpp"

using namespace gatehaven;

TEST("screen-switch example demonstrates two distinct signal delays") {
    const auto c = screen_switch_circuit(); Simulation sim;
    const Simulation::Exchange held = [](const CommunicatorGroup& group, bool) { return group.id == Point{0, 0}; };
    sim.step(c, held); CHECK(!sim.powered({6, 0}) && !sim.sent({10, 0}));
    sim.step(c, held); CHECK(sim.powered({6, 0}) && !sim.sent({10, 0}));
    sim.step(c, held); CHECK(sim.sent({10, 0}));
}
TEST("relay examples expose opposite control behavior without accidental power bypass") {
    for (const bool negative : {false, true}) for (const bool pressed : {false, true}) {
        const auto c = relay_demo_circuit(negative); Simulation sim;
        const Simulation::Exchange input = [&](const CommunicatorGroup& group, bool) { return pressed && group.id == Point{0, 4}; };
        sim.step(c, input); sim.step(c, input);
        CHECK(sim.powered({3, 0}) == (negative != pressed));
    }
}
TEST("gate gallery reproduces all two-input truth-table combinations") {
    const auto c = gate_gallery_circuit();
    for (unsigned mask = 0; mask < 4; ++mask) {
        Simulation sim;
        const Simulation::Exchange input = [&](const CommunicatorGroup& group, bool) {
            if (group.id.y != 0) return false;
            const auto position = ((group.id.x % 10) + 10) % 10;
            return position == 7 ? (mask & 1) != 0 : (mask & 2) != 0;
        };
        sim.step(c, input); sim.step(c, input);
        CHECK(sim.powered({0, -1}) == (mask == 3));
        CHECK(sim.powered({10, -1}) == (mask != 0));
        CHECK(sim.powered({20, -1}) == (mask != 3));
        CHECK(sim.powered({30, -1}) == (mask == 0));
    }
}

TEST("starter output turns on after two steps and its crossing stays isolated") {
    const auto circuit = starter_circuit();
    Simulation sim;
    sim.step(circuit);
    CHECK(!sim.powered({8, 0}));
    sim.step(circuit);
    CHECK(sim.powered({8, 0}));
    CHECK(sim.powered({8, 7}));
    CHECK(!sim.powered({0, 10}));
}

TEST("feedback oscillator alternates without depending on real time") {
    const auto circuit = oscillator_circuit();
    Simulation sim;
    for (unsigned i = 0; i < 100; ++i) {
        sim.step(circuit);
        CHECK(sim.powered({0, 0}) == (i % 2 == 0));
    }
}
