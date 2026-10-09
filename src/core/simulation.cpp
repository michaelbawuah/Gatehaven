#include "gatehaven/simulation.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace gatehaven {
namespace {

constexpr bool control(Element element, unsigned on, unsigned count) {
    switch (element) {
    case Element::positive_relay:
    case Element::or_gate: return on != 0;
    case Element::negative_relay: return on < count;
    case Element::and_gate: return on == count;
    case Element::nand_gate: return on != count;
    case Element::nor_gate: return on == 0;
    default: return false;
    }
}


constexpr bool signal_connects(Element element) {
    return element == Element::wire || element == Element::crossing || element == Element::signal;
}

constexpr bool connects(Element a, Element b) {
    return (a != Element::signal || signal_connects(b)) &&
           (b != Element::signal || signal_connects(a));
}

} // namespace

Simulation::Material Simulation::material(Element element, bool enabled) {
    switch (element) {
    case Element::empty: return Material::blocked;
    case Element::crossing: return Material::crossing;
    case Element::source: return Material::source;
    case Element::wire:
    case Element::signal: return Material::conductor;
    case Element::positive_relay:
    case Element::negative_relay: return enabled ? Material::conductor : Material::blocked;
    case Element::and_gate:
    case Element::or_gate:
    case Element::nand_gate:
    case Element::nor_gate:
    case Element::screen:
    case Element::file_input:
    case Element::file_output: return enabled ? Material::source : Material::conductor;
    }
    return Material::blocked;
}

void Simulation::propagate() {
    const auto& nodes = topology_.nodes();
    for (std::size_t head = 0; head < frontier_.size(); ++head) {
        const auto [from, outgoing] = frontier_[head];
        for (std::size_t direction = 0; direction < 4; ++direction) {
            if ((outgoing & (1U << direction)) == 0) continue;
            const auto next = nodes[from].adjacent[direction];
            if (next == no_node || materials_[next] == Material::blocked || !connects(nodes[from].cell.element, nodes[next].cell.element)) continue;
            const auto channel = materials_[next] == Material::crossing
                ? static_cast<std::uint8_t>(direction % 2 == 0 ? 5 : 10) : std::uint8_t{15};
            const auto added = static_cast<std::uint8_t>(channel & static_cast<std::uint8_t>(~power_[next]));
            if (added == 0) continue;
            power_[next] = static_cast<std::uint8_t>(power_[next] | added);
            frontier_.emplace_back(next, added);
        }
    }
    metrics_.frontier_visits += frontier_.size();
}

void Simulation::initialize(const Circuit& circuit, bool reset_levels) {
    reset();
    topology_ = CompiledCircuit(circuit);
    topology_revision_ = circuit.revision();
    const auto& nodes = topology_.nodes();
    power_.assign(nodes.size(), 0); previous_.clear();
    sent_.assign(nodes.size(), false); received_.assign(nodes.size(), false);
    materials_.resize(nodes.size()); valid_.assign(nodes.size(), true);
    frontier_.clear(); frontier_.reserve(nodes.size());
    const auto mask = reset_levels ? 1U : 2U;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        materials_[i] = material(nodes[i].cell.element, (nodes[i].cell.state & mask) != 0);
        if (materials_[i] == Material::source) { power_[i] = 15; frontier_.emplace_back(i, std::uint8_t{15}); }
    }
    metrics_.topology_builds = 1; metrics_.propagations = 1;
    propagate(); snapshot_dirty_ = true;
}

void Simulation::refresh(const Circuit& circuit) {
    const auto ticks = ticks_;
    const auto metrics = metrics_;
    initialize(document_snapshot(circuit));
    topology_revision_ = circuit.revision();
    ticks_ = ticks;
    metrics_.topology_builds += metrics.topology_builds;
    metrics_.propagations += metrics.propagations;
    metrics_.frontier_visits += metrics.frontier_visits;
    metrics_.settled_ticks += metrics.settled_ticks;
}

bool Simulation::conductive(Point point) const {
    const auto index = topology_.index(point);
    if (index == no_node || index >= materials_.size()) return false;
    const auto element = topology_.nodes()[index].cell.element;
    return (element == Element::positive_relay || element == Element::negative_relay) && materials_[index] == Material::conductor;
}

Circuit Simulation::document_snapshot(const Circuit& circuit) const {
    Circuit snapshot = circuit;
    for (const auto& cell : circuit.cells()) {
        const auto index = topology_.index(cell.position);
        if (index == no_node || index >= valid_.size() || !valid_[index] || topology_.nodes()[index].cell.element != cell.element) continue;
        const bool relay = cell.element == Element::positive_relay || cell.element == Element::negative_relay;
        const bool high = relay ? conductive(cell.position) : cell.element != Element::source && powered(cell.position);
        snapshot.set(cell.position, cell.element, static_cast<std::uint8_t>((cell.state & 1U) | (high ? 2U : 0U)));
    }
    return snapshot;
}

void Simulation::step(const Circuit& circuit, const Exchange& exchange) {
    if (topology_revision_ != circuit.revision()) {
        CompiledCircuit rebuilt(circuit);
        std::vector<std::uint8_t> preserved(rebuilt.nodes().size());
        for (std::size_t i = 0; i < rebuilt.nodes().size(); ++i) {
            const auto& cell = rebuilt.nodes()[i].cell;
            const auto old = topology_.index(cell.position);
            if (old != no_node && topology_.nodes()[old].cell.element == cell.element) preserved[i] = power_[old];
        }
        topology_ = std::move(rebuilt); power_ = std::move(preserved);
        topology_revision_ = circuit.revision();
        ++metrics_.topology_builds; settled_ = false;
    }
    if (settled_) { ++ticks_; ++metrics_.settled_ticks; return; }
    ++metrics_.propagations;
    const auto& nodes = topology_.nodes();
    power_.swap(previous_);
    power_.assign(nodes.size(), 0);
    materials_.resize(nodes.size());
    received_.assign(nodes.size(), false);
    frontier_.clear(); frontier_.reserve(nodes.size()); sent_.assign(nodes.size(), false);
    for (const auto& group : topology_.groups()) {
        bool sending = false;
        for (const auto input : group.inputs) sending = sending || previous_[input] != 0;
        const bool receiving = exchange && exchange(group.endpoint, sending);
        for (const auto member : group.members) {
            received_[member] = receiving; sent_[member] = sending;
        }
    }
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto element = nodes[i].cell.element;
        unsigned inputs = 0, active = 0;
        if (element >= Element::positive_relay && !is_communicator(element)) {
            for (const auto adjacent : nodes[i].adjacent) {
                if (adjacent == no_node || nodes[adjacent].cell.element != Element::signal) continue;
                ++inputs; if (previous_[adjacent] != 0) ++active;
            }
        }
        materials_[i] = material(element, is_communicator(element) ? received_[i] : control(element, active, inputs));
        if (materials_[i] == Material::source) { power_[i] = 15; frontier_.emplace_back(i, std::uint8_t{15}); }
    }
    propagate();
    settled_ = topology_.groups().empty() && power_ == previous_;
    valid_.assign(nodes.size(), true); snapshot_dirty_ = true;
    ++ticks_;
}

void Simulation::reset() {
    state_.clear();
    power_.assign(topology_.nodes().size(), 0);
    sent_.assign(power_.size(), false); received_.assign(power_.size(), false); valid_.assign(power_.size(), false);
    snapshot_dirty_ = false;
    ticks_ = 0; metrics_ = {}; settled_ = false;
}

void Simulation::invalidate(std::span<const Point> points) {
    if (!points.empty()) settled_ = false;
    for (const auto point : points) {
        const auto index = topology_.index(point);
        if (index != no_node) { power_[index] = 0; sent_[index] = false; received_[index] = false; valid_[index] = false; snapshot_dirty_ = true; }
    }
}

std::uint8_t Simulation::ports(Point point) const {
    const auto index = topology_.index(point);
    return index == no_node ? std::uint8_t{0} : power_[index];
}

std::size_t Simulation::powered_count() const {
    return static_cast<std::size_t>(std::count_if(power_.begin(), power_.end(), [](auto value) { return value != 0; }));
}

bool Simulation::sent(Point point) const {
    const auto index = topology_.index(point);
    return index != no_node && sent_[index];
}

bool Simulation::received(Point point) const {
    const auto index = topology_.index(point);
    return index != no_node && received_[index];
}

const std::map<Point, Power>& Simulation::snapshot() const {
    if (snapshot_dirty_) {
        state_.clear();
        const auto& nodes = topology_.nodes();
        for (std::size_t i = 0; i < nodes.size(); ++i) if (valid_[i]) {
            state_.emplace_hint(state_.end(), nodes[i].cell.position, Power{nodes[i].cell.element, power_[i]});
        }
        snapshot_dirty_ = false;
    }
    return state_;
}

} // namespace gatehaven
