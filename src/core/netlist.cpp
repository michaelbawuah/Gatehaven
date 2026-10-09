#include "gatehaven/netlist.hpp"
#include "gatehaven/connectivity.hpp"
#include <algorithm>
#include <numeric>

namespace gatehaven {
namespace {
class Components {
public:
    explicit Components(std::size_t size) : parent_(size), rank_(size) { std::iota(parent_.begin(), parent_.end(), 0); }
    std::size_t root(std::size_t value) {
        while (parent_[value] != value) { parent_[value] = parent_[parent_[value]]; value = parent_[value]; }
        return value;
    }
    void join(std::size_t a, std::size_t b) {
        a = root(a); b = root(b); if (a == b) return;
        if (rank_[a] < rank_[b]) std::swap(a, b);
        parent_[b] = a;
        if (rank_[a] == rank_[b]) ++rank_[a];
    }
private:
    std::vector<std::size_t> parent_;
    std::vector<unsigned char> rank_;
};
void unique(std::vector<std::size_t>& values) {
    std::sort(values.begin(), values.end()); values.erase(std::unique(values.begin(), values.end()), values.end());
}
}
Netlist::Netlist(const CompiledCircuit& topology) {
    const auto& nodes = topology.nodes();
    Components components(nodes.size() * 2);
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto element = nodes[i].cell.element;
        if (is_relay(element)) continue;
        if (element != Element::crossing) components.join(i * 2, i * 2 + 1);
        for (const auto d : {1U, 2U}) {
            const auto next = nodes[i].adjacent[d];
            if (next == no_node || is_relay(nodes[next].cell.element) || !conducts_between(element, nodes[next].cell.element)) continue;
            const auto axis = d % 2 == 0 ? 1U : 0U;
            components.join(i * 2 + axis, next * 2 + axis);
        }
    }
    terminals_.resize(nodes.size());
    std::vector<std::size_t> identities(nodes.size() * 2, no_node);
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (is_relay(nodes[i].cell.element)) continue;
        for (unsigned axis = 0; axis < 2; ++axis) {
            auto& id = identities[components.root(i * 2 + axis)];
            if (id == no_node) id = fixed_count_++;
            terminals_[i][axis] = id;
        }
    }
    auto vertices = fixed_count_;
    for (std::size_t i = 0; i < nodes.size(); ++i) if (is_relay(nodes[i].cell.element)) terminals_[i] = {vertices, vertices}, ++vertices;
    links_.resize(vertices);
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto element = nodes[i].cell.element;
        if (element == Element::source) sources_.push_back(terminals_[i][0]);
        if (receives_signal(element) && !is_communicator(element)) {
            NetControl control{element, terminals_[i][0]};
            control.node = i;
            control.seek_high = element == Element::positive_relay || element == Element::or_gate || element == Element::nor_gate;
            control.invert = element == Element::and_gate || element == Element::nor_gate;
            for (const auto next : nodes[i].adjacent) if (next != no_node && nodes[next].cell.element == Element::signal) control.inputs[control.count++] = terminals_[next][0];
            controls_.push_back(control);
        }
        if (!is_relay(element)) continue;
        const auto relay = terminals_[i][0];
        for (unsigned d = 0; d < 4; ++d) {
            const auto next = nodes[i].adjacent[d];
            if (next == no_node || !conducts_between(element, nodes[next].cell.element)) continue;
            const auto target = terminals_[next][d % 2 == 0 ? 1 : 0];
            links_[relay].push_back(target); links_[target].push_back(relay);
        }
    }
    std::stable_sort(controls_.begin(), controls_.end(), [](const auto& a, const auto& b) {
        return std::tie(a.element, a.count) < std::tie(b.element, b.count);
    });
    unique(sources_);
    for (auto& links : links_) unique(links);
    node_groups_.assign(nodes.size(), no_node);
    for (std::size_t group = 0; group < topology.groups().size(); ++group) {
        auto& inputs = group_inputs_.emplace_back();
        for (const auto node : topology.groups()[group].inputs) inputs.push_back(terminals_[node][0]);
        unique(inputs);
        auto& outputs = group_outputs_.emplace_back();
        for (const auto node : topology.groups()[group].members) { outputs.push_back(terminals_[node][0]); node_groups_[node] = group; }
        unique(outputs);
    }
}
}
