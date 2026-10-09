#include "gatehaven/topology.hpp"
#include <algorithm>

namespace gatehaven {
CompiledCircuit::CompiledCircuit(const Circuit& circuit) {
    nodes_.reserve(circuit.size());
    if (const auto bounds = circuit.bounds()) circuit.visit(*bounds, [&](Cell cell) { nodes_.push_back({cell}); });
    // Rows and columns are ordered. Link adjacent columns directly, then merge
    // each pair of consecutive rows in linear time without coordinate hashing.
    for (std::size_t i = 1; i < nodes_.size(); ++i) {
        const auto previous = nodes_[i - 1].cell.position, current = nodes_[i].cell.position;
        if (previous.y == current.y && static_cast<std::int64_t>(previous.x) + 1 == current.x) {
            nodes_[i - 1].adjacent[1] = i; nodes_[i].adjacent[3] = i - 1;
        }
    }
    for (std::size_t row = 0; row < nodes_.size();) {
        std::size_t next = row + 1;
        while (next < nodes_.size() && nodes_[next].cell.position.y == nodes_[row].cell.position.y) ++next;
        if (next == nodes_.size()) break;
        std::size_t end = next + 1;
        while (end < nodes_.size() && nodes_[end].cell.position.y == nodes_[next].cell.position.y) ++end;
        if (static_cast<std::int64_t>(nodes_[row].cell.position.y) + 1 == nodes_[next].cell.position.y) {
            std::size_t upper = row, lower = next;
            while (upper < next && lower < end) {
                const auto x = nodes_[upper].cell.position.x, other = nodes_[lower].cell.position.x;
                if (x < other) ++upper;
                else if (other < x) ++lower;
                else { nodes_[upper].adjacent[2] = lower; nodes_[lower].adjacent[0] = upper; ++upper; ++lower; }
            }
        }
        row = next;
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
