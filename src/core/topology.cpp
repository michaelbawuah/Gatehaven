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
    std::vector<bool> visited(nodes_.size());
    for (std::size_t seed = 0; seed < nodes_.size(); ++seed) {
        if (!is_communicator(nodes_[seed].cell.element) || visited[seed]) continue;
        TopologyGroup group{{nodes_[seed].cell.position, nodes_[seed].cell.element, {}}, {seed}, {}};
        visited[seed] = true;
        for (std::size_t head = 0; head < group.members.size(); ++head) {
            for (const auto next : nodes_[group.members[head]].adjacent) {
                if (next == no_node) continue;
                if (nodes_[next].cell.element == Element::signal) group.inputs.push_back(next);
                if (nodes_[next].cell.element == group.endpoint.element && !visited[next]) {
                    visited[next] = true; group.members.push_back(next);
                }
            }
        }
        std::sort(group.members.begin(), group.members.end());
        for (const auto member : group.members) group.endpoint.cells.push_back(nodes_[member].cell.position);
        std::sort(group.inputs.begin(), group.inputs.end());
        group.inputs.erase(std::unique(group.inputs.begin(), group.inputs.end()), group.inputs.end());
        groups_.push_back(std::move(group));
    }

}
std::size_t CompiledCircuit::index(Point point) const {
    const auto found = std::lower_bound(nodes_.begin(), nodes_.end(), point,
        [](const TopologyNode& node, Point value) { return node.cell.position < value; });
    return found != nodes_.end() && found->cell.position == point ? static_cast<std::size_t>(found - nodes_.begin()) : no_node;
}
}
