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

TEST("linear row joins retain sparse gaps and bidirectional adjacency") {
    Circuit circuit;
    for (const Point p : {Point{-4, -3}, {-3, -3}, {8, -3}, {-4, -2}, {7, -2}, {8, -2}, {8, 0}}) circuit.set(p, Element::wire);
    CompiledCircuit topology(circuit);
    for (const auto& node : topology.nodes()) for (unsigned d = 0; d < 4; ++d) {
        const auto point = neighbor(node.cell.position, directions[d]);
        CHECK(node.adjacent[d] == (point ? topology.index(*point) : no_node));
    }
}

TEST("compiled communicator flood fill matches the public grouping contract") {
    Circuit circuit;
    for (Coordinate y = -3; y < 6; ++y) for (Coordinate x = -4; x < 8; ++x)
        circuit.set({x, y}, ((x * x + y * y) % 3 == 0) ? Element::screen : Element::file_input);
    const auto expected = communicator_groups(circuit);
    CompiledCircuit topology(circuit); CHECK(expected.size() == topology.groups().size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(expected[i].id == topology.groups()[i].endpoint.id);
        CHECK(expected[i].element == topology.groups()[i].endpoint.element);
        CHECK(expected[i].cells == topology.groups()[i].endpoint.cells);
    }
}
