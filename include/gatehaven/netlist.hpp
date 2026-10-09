#pragma once
#include "gatehaven/topology.hpp"

namespace gatehaven {
struct NetControl {
    Element element;
    std::size_t output;
    std::array<std::size_t, 4> inputs{no_node, no_node, no_node, no_node};
    std::array<std::size_t, 4> input_nodes{no_node, no_node, no_node, no_node};
    unsigned count{};
};
// Fixed wire components are vertices; each relay is one independently switched
// vertex. Crossings have separate horizontal and vertical component identifiers.
class Netlist {
public:
    Netlist() = default;
    explicit Netlist(const CompiledCircuit& topology);
    [[nodiscard]] const auto& terminals() const { return terminals_; }
    [[nodiscard]] const auto& links() const { return links_; }
    [[nodiscard]] const auto& controls() const { return controls_; }
    [[nodiscard]] const auto& sources() const { return sources_; }
    [[nodiscard]] const auto& group_outputs() const { return group_outputs_; }
    [[nodiscard]] const auto& group_inputs() const { return group_inputs_; }
    [[nodiscard]] const auto& node_groups() const { return node_groups_; }
    [[nodiscard]] std::size_t fixed_count() const { return fixed_count_; }
    [[nodiscard]] std::size_t vertex_count() const { return links_.size(); }
private:
    std::vector<std::array<std::size_t, 2>> terminals_;
    std::vector<std::vector<std::size_t>> links_;
    std::vector<NetControl> controls_;
    std::vector<std::size_t> sources_;
    std::vector<std::vector<std::size_t>> group_outputs_;
    std::vector<std::vector<std::size_t>> group_inputs_;
    std::vector<std::size_t> node_groups_;
    std::size_t fixed_count_{};
};
}
