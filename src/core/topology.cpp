#include "gatehaven/topology.hpp"
#include <algorithm>

namespace gatehaven {
CompiledCircuit::CompiledCircuit(const Circuit& circuit) {
    nodes_.reserve(circuit.size());
    for (const auto& cell : circuit.cells()) nodes_.push_back({cell});
    for (auto& node : nodes_) for (std::size_t direction = 0; direction < directions.size(); ++direction) {
        if (const auto next = neighbor(node.cell.position, directions[direction])) node.adjacent[direction] = index(*next);
    }
    for (auto group : communicator_groups(circuit)) {
        TopologyGroup compiled{std::move(group), {}, {}};
        compiled.members.reserve(compiled.endpoint.cells.size());
        for (const auto point : compiled.endpoint.cells) {
            const auto member = index(point); compiled.members.push_back(member);
            for (const auto adjacent : nodes_[member].adjacent) {
                if (adjacent != no_node && nodes_[adjacent].cell.element == Element::signal) compiled.inputs.push_back(adjacent);
            }
        }
        std::sort(compiled.inputs.begin(), compiled.inputs.end());
        compiled.inputs.erase(std::unique(compiled.inputs.begin(), compiled.inputs.end()), compiled.inputs.end());
        groups_.push_back(std::move(compiled));
    }
}
std::size_t CompiledCircuit::index(Point point) const {
    const auto found = std::lower_bound(nodes_.begin(), nodes_.end(), point,
        [](const TopologyNode& node, Point value) { return node.cell.position < value; });
    return found != nodes_.end() && found->cell.position == point ? static_cast<std::size_t>(found - nodes_.begin()) : no_node;
}
}
