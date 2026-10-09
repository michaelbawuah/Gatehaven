#include "test.hpp"
#include "gatehaven/topology.hpp"
using namespace gatehaven;
TEST("compiled topology keeps deterministic adjacency without wrapping coordinate boundaries") {
    Circuit circuit;
    circuit.set({0, 0}, Element::wire); circuit.set({1, 0}, Element::crossing);
    circuit.set({std::numeric_limits<Coordinate>::min(), 0}, Element::source);
    circuit.set({std::numeric_limits<Coordinate>::max(), 0}, Element::wire);
    CompiledCircuit topology(circuit);
    CHECK(topology.nodes().size() == 4);
    CHECK(topology.nodes()[topology.index({0, 0})].adjacent[1] == topology.index({1, 0}));
    CHECK(topology.nodes().front().adjacent[3] == no_node);
    CHECK(topology.nodes().back().adjacent[1] == no_node);
    CHECK(topology.index({99, 99}) == no_node);
}
TEST("compiled communicator groups collect each neighboring Signal only once") {
    Circuit circuit;
    circuit.set({0, 0}, Element::screen); circuit.set({1, 0}, Element::screen);
    circuit.set({0, 1}, Element::screen); circuit.set({1, 1}, Element::signal);
    CompiledCircuit topology(circuit);
    CHECK(topology.groups().size() == 1);
    CHECK(topology.groups()[0].members.size() == 3);
    CHECK(topology.groups()[0].inputs == std::vector<std::size_t>{topology.index({1, 1})});
}
