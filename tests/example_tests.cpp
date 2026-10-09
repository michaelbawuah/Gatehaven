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
