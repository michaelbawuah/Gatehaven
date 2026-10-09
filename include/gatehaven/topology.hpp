#pragma once
#include "gatehaven/communicator.hpp"
#include <array>
#include <limits>

namespace gatehaven {
inline constexpr std::size_t no_node = std::numeric_limits<std::size_t>::max();
struct TopologyNode {
    Cell cell;
    std::array<std::size_t, 4> adjacent{no_node, no_node, no_node, no_node};
};
struct TopologyGroup {
    CommunicatorGroup endpoint;
    std::vector<std::size_t> members;
    std::vector<std::size_t> inputs;
};
class CompiledCircuit {
public:
    CompiledCircuit() = default;
    explicit CompiledCircuit(const Circuit& circuit);
    [[nodiscard]] std::size_t index(Point point) const;
    [[nodiscard]] const std::vector<TopologyNode>& nodes() const { return nodes_; }
    [[nodiscard]] const std::vector<TopologyGroup>& groups() const { return groups_; }
private:
    std::vector<TopologyNode> nodes_;
    std::vector<TopologyGroup> groups_;
};
}
