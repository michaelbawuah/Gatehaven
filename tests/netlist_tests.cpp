#include "test.hpp"
#include "gatehaven/netlist.hpp"
using namespace gatehaven;
TEST("fixed nets collapse long wires while crossing channels stay separate") {
    Circuit circuit;
    for (Coordinate x = -100; x <= 100; ++x) circuit.set({x, 0}, Element::wire);
    circuit.set({0, 0}, Element::crossing); circuit.set({0, -1}, Element::wire); circuit.set({0, 1}, Element::wire);
    CompiledCircuit topology(circuit); Netlist nets(topology);
    CHECK(nets.fixed_count() == 2 && nets.vertex_count() == 2);
    const auto crossing = nets.terminals()[topology.index({0, 0})]; CHECK(crossing[0] != crossing[1]);
    CHECK(crossing[0] == nets.terminals()[topology.index({100, 0})][0]);
    CHECK(crossing[1] == nets.terminals()[topology.index({0, 1})][0]);
}
TEST("relays remain switched vertices and signal edges become control inputs") {
    Circuit circuit; circuit.set({-1, 0}, Element::source); circuit.set({0, 0}, Element::positive_relay);
    circuit.set({1, 0}, Element::wire); circuit.set({0, -1}, Element::signal);
    CompiledCircuit topology(circuit); Netlist nets(topology);
    CHECK(nets.fixed_count() == 3 && nets.vertex_count() == 4);
    const auto relay = nets.terminals()[topology.index({0, 0})][0]; CHECK(nets.links()[relay].size() == 2);
    CHECK(nets.controls().size() == 1 && nets.controls()[0].count == 1);
    CHECK(nets.controls()[0].input_nodes[0] == topology.index({0, -1}));
}
TEST("communicator outputs are deduplicated without merging endpoint types") {
    Circuit circuit; circuit.set({0, 0}, Element::screen); circuit.set({1, 0}, Element::screen);
    circuit.set({2, 0}, Element::file_input);
    CompiledCircuit topology(circuit); Netlist nets(topology);
    CHECK(nets.fixed_count() == 1 && nets.group_outputs().size() == 2);
    CHECK(nets.group_outputs()[0].size() == 1 && nets.group_outputs()[1].size() == 1);
}
