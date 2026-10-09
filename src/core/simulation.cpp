#include "gatehaven/simulation.hpp"
#include "gatehaven/connectivity.hpp"
#include <algorithm>

namespace gatehaven {
void Simulation::rebuild(const Circuit& circuit) {
    topology_ = CompiledCircuit(circuit);
    nets_ = Netlist(topology_);
    topology_revision_ = circuit.revision();
    power_.assign(nets_.vertex_count(), 0); previous_.clear();
    enabled_.assign(nets_.vertex_count(), 1);
    std::fill(enabled_.begin() + static_cast<std::ptrdiff_t>(nets_.fixed_count()), enabled_.end(), std::uint8_t{0});
    valid_.assign(topology_.nodes().size(), false);
    sent_.assign(topology_.groups().size(), false); received_.assign(sent_.size(), false);
    frontier_.clear(); frontier_.reserve(nets_.vertex_count());
    ++metrics_.topology_builds; settled_ = false; snapshot_dirty_ = true;
}
void Simulation::energize(std::size_t vertex) {
    if (enabled_[vertex] && !power_[vertex]) { power_[vertex] = 1; frontier_.push_back(vertex); }
}
void Simulation::propagate() {
    for (std::size_t head = 0; head < frontier_.size(); ++head)
        for (const auto next : nets_.links()[frontier_[head]]) energize(next);
    metrics_.frontier_visits += frontier_.size();
}
std::uint8_t Simulation::node_power(std::size_t node) const {
    if (node >= valid_.size() || !valid_[node]) return 0;
    const auto [horizontal, vertical] = nets_.terminals()[node];
    return static_cast<std::uint8_t>((power_[horizontal] ? 10 : 0) | (power_[vertical] ? 5 : 0));
}
bool Simulation::group_flag(std::size_t node, const std::vector<bool>& flags) const {
    if (node >= valid_.size() || !valid_[node]) return false;
    const auto group = nets_.node_groups()[node];
    return group != no_node && flags[group];
}
void Simulation::initialize(const Circuit& circuit, bool reset_levels) {
    reset(); rebuild(circuit);
    const auto& nodes = topology_.nodes();
    const auto mask = reset_levels ? 1U : 2U;
    // Relay openness must be established before any sources are propagated.
    for (std::size_t i = 0; i < nodes.size(); ++i) if (is_relay(nodes[i].cell.element))
        enabled_[nets_.terminals()[i][0]] = static_cast<std::uint8_t>((nodes[i].cell.state & mask) != 0);
    for (const auto source : nets_.sources()) energize(source);
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& cell = nodes[i].cell;
        if (receives_signal(cell.element) && !is_relay(cell.element) && (cell.state & mask)) energize(nets_.terminals()[i][0]);
    }
    ++metrics_.propagations; propagate();
    valid_.assign(nodes.size(), true); snapshot_dirty_ = true; invalidated_ = false;
}
void Simulation::refresh(const Circuit& circuit) {
    const auto ticks = ticks_; const auto metrics = metrics_;
    initialize(document_snapshot(circuit)); topology_revision_ = circuit.revision(); ticks_ = ticks;
    metrics_.topology_builds += metrics.topology_builds; metrics_.propagations += metrics.propagations;
    metrics_.frontier_visits += metrics.frontier_visits; metrics_.settled_ticks += metrics.settled_ticks;
}
void Simulation::step(const Circuit& circuit, const Exchange& exchange) {
    const bool changed = topology_revision_ != circuit.revision();
    std::vector<std::uint8_t> old_nodes;
    if (changed) {
        old_nodes.reserve(circuit.size());
        if (const auto bounds = circuit.bounds()) circuit.visit(*bounds, [&](const Cell& cell) {
            const auto old = topology_.index(cell.position);
            old_nodes.push_back(old != no_node && topology_.nodes()[old].cell.element == cell.element ? node_power(old) : std::uint8_t{0});
        });
        rebuild(circuit);
    }
    if (settled_ && (!exchange || topology_.groups().empty())) { ++ticks_; ++metrics_.settled_ticks; return; }
    const auto& nodes = topology_.nodes();
    power_.swap(previous_); power_.assign(nets_.vertex_count(), 0); frontier_.clear();
    const auto previous_power = [&](std::size_t node) {
        if (changed) return old_nodes[node] != 0;
        if (!valid_[node]) return false;
        const auto terminal = nets_.terminals()[node];
        return previous_[terminal[0]] || previous_[terminal[1]];
    };
    const auto evaluate = [&]<bool NodeInputs>(auto sample) {
        for (const auto& gate : nets_.controls()) {
            bool found = false;
            if constexpr (NodeInputs) {
                for (const auto input : nodes[gate.node].adjacent) {
                    if (input != no_node && nodes[input].cell.element == Element::signal && sample(input) == gate.seek_high) { found = true; break; }
                }
            } else {
                for (unsigned i = 0; i < gate.count; ++i) if (sample(gate.inputs[i]) == gate.seek_high) { found = true; break; }
            }
            const bool on = found != gate.invert;
            if (is_relay(gate.element)) enabled_[gate.output] = static_cast<std::uint8_t>(on);
            else if (on) energize(gate.output);
        }
        for (std::size_t group = 0; group < topology_.groups().size(); ++group) {
            bool sending = false;
            const auto& inputs = NodeInputs ? topology_.groups()[group].inputs : nets_.group_inputs()[group];
            for (const auto input : inputs) sending = sending || sample(input);
            sent_[group] = sending;
            received_[group] = exchange && exchange(topology_.groups()[group].endpoint, sending);
            if (received_[group]) for (const auto output : nets_.group_outputs()[group]) energize(output);
        }
    };
    if (changed || invalidated_) evaluate.template operator()<true>(previous_power);
    else evaluate.template operator()<false>([&](std::size_t vertex) { return previous_[vertex] != 0; });
    for (const auto source : nets_.sources()) energize(source);
    ++metrics_.propagations; propagate();
    // A rebuilt graph has a new component partition: comparisons become valid
    // only after its first complete tick.
    settled_ = !changed && (!exchange || topology_.groups().empty()) && power_ == previous_;
    valid_.assign(nodes.size(), true); snapshot_dirty_ = true; invalidated_ = false; ++ticks_;
}
void Simulation::reset() {
    state_.clear(); power_.assign(nets_.vertex_count(), 0);
    sent_.assign(topology_.groups().size(), false); received_.assign(sent_.size(), false);
    valid_.assign(topology_.nodes().size(), false);
    snapshot_dirty_ = false; ticks_ = 0; metrics_ = {}; settled_ = false; invalidated_ = true;
}
void Simulation::invalidate(std::span<const Point> points) {
    if (!points.empty()) { settled_ = false; invalidated_ = true; }
    for (const auto point : points) {
        const auto index = topology_.index(point);
        if (index != no_node) { valid_[index] = false; snapshot_dirty_ = true; }
    }
}
std::uint8_t Simulation::ports(Point point) const {
    const auto index = topology_.index(point); return index == no_node ? std::uint8_t{0} : node_power(index);
}
bool Simulation::conductive(Point point) const {
    const auto index = topology_.index(point);
    return index != no_node && valid_[index] && is_relay(topology_.nodes()[index].cell.element) && enabled_[nets_.terminals()[index][0]];
}
std::size_t Simulation::powered_count() const {
    std::size_t count = 0;
    for (std::size_t i = 0; i < topology_.nodes().size(); ++i) if (node_power(i)) ++count;
    return count;
}
bool Simulation::sent(Point point) const { const auto index = topology_.index(point); return index != no_node && group_flag(index, sent_); }
bool Simulation::received(Point point) const { const auto index = topology_.index(point); return index != no_node && group_flag(index, received_); }
const std::map<Point, Power>& Simulation::snapshot() const {
    if (snapshot_dirty_) {
        state_.clear();
        visit_state([&](Point point, Power power, bool, bool) { state_.emplace_hint(state_.end(), point, power); });
        snapshot_dirty_ = false;
    }
    return state_;
}
Circuit Simulation::document_snapshot(const Circuit& circuit) const {
    Circuit snapshot = circuit;
    if (const auto bounds = circuit.bounds()) circuit.visit(*bounds, [&](const Cell& cell) {
        const auto index = topology_.index(cell.position);
        if (index == no_node || !valid_[index] || topology_.nodes()[index].cell.element != cell.element) return;
        const bool high = is_relay(cell.element) ? conductive(cell.position) : cell.element != Element::source && powered(cell.position);
        snapshot.set(cell.position, cell.element, static_cast<std::uint8_t>((cell.state & 1U) | (high ? 2U : 0U)));
    });
    return snapshot;
}
}
