#include "test.hpp"
#include "gatehaven/communicator.hpp"
#include "gatehaven/simulation.hpp"
using namespace gatehaven;
TEST("communicator groups join only adjacent cells of the same type") {
    Circuit c;
    c.set({0, 0}, Element::screen); c.set({1, 0}, Element::screen);
    c.set({2, 0}, Element::file_input); c.set({3, 0}, Element::screen);
    const auto groups = communicator_groups(c);
    CHECK(groups.size() == 3); CHECK(groups[0].cells.size() == 2);
    CHECK(groups[0].id == Point{0, 0}); CHECK(groups[1].element == Element::file_input);
    c.set({1, 0}, Element::empty);
    CHECK(communicator_groups(c)[0].cells.size() == 1);
}
TEST("communicators exchange once per group and send only previous-step signals") {
    Circuit c;
    c.set({0, 0}, Element::source); c.set({1, 0}, Element::wire); c.set({2, 0}, Element::signal);
    c.set({3, 0}, Element::screen); c.set({4, 0}, Element::screen); c.set({5, 0}, Element::wire);
    Simulation sim; unsigned calls = 0; bool sent = false;
    const Simulation::Exchange endpoint = [&](const CommunicatorGroup& group, bool value) {
        ++calls; CHECK(group.cells.size() == 2); sent = value; return true;
    };
    sim.step(c, endpoint); CHECK(calls == 1 && !sent && sim.powered({5, 0}));
    sim.step(c, endpoint); CHECK(calls == 2 && sent && sim.sent({4, 0}));
    sim.reset(); CHECK(!sim.sent({4, 0}));
    sim.step(c); CHECK(!sim.powered({5, 0}));
}

TEST("cached communicator groups refresh after shape edits and circuit replacement") {
    Circuit first; first.set({0, 0}, Element::screen); first.set({2, 0}, Element::screen);
    Simulation sim; unsigned calls = 0;
    const Simulation::Exchange exchange = [&](const CommunicatorGroup&, bool) { ++calls; return true; };
    sim.step(first, exchange); sim.step(first, exchange); CHECK(calls == 4);
    first.set({1, 0}, Element::screen); calls = 0; sim.step(first, exchange); CHECK(calls == 1);
    Circuit second; second.set({9, 9}, Element::file_input);
    first = second; calls = 0; sim.step(first, exchange); CHECK(calls == 1 && sim.powered({9, 9}));
    first.clear(); calls = 0; sim.step(first, exchange); CHECK(calls == 0);
}
