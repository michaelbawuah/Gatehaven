#include "gatehaven/simulation.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace gatehaven {
namespace {

enum class Material { blocked, conductor, crossing, source };

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

constexpr Material material(Element element, bool enabled) {
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

constexpr bool signal_connects(Element element) {
    return element == Element::wire || element == Element::crossing || element == Element::signal;
}

constexpr bool connects(Element a, Element b) {
    return (a != Element::signal || signal_connects(b)) &&
           (b != Element::signal || signal_connects(a));
}

} // namespace

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
    }
    const auto& nodes = topology_.nodes();
    const auto previous = power_;
    power_.assign(nodes.size(), 0);
    std::vector<Material> materials(nodes.size());
    std::vector<bool> received(nodes.size());
    std::vector<std::pair<std::size_t, std::uint8_t>> frontier;
    frontier.reserve(nodes.size()); sent_.assign(nodes.size(), false);
    for (const auto& group : topology_.groups()) {
        bool sending = false;
        for (const auto input : group.inputs) sending = sending || previous[input] != 0;
        const bool receiving = exchange && exchange(group.endpoint, sending);
        for (const auto member : group.members) {
            received[member] = receiving; sent_[member] = sending;
        }
    }
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto element = nodes[i].cell.element;
        unsigned inputs = 0, active = 0;
        if (element >= Element::positive_relay && !is_communicator(element)) {
            for (const auto adjacent : nodes[i].adjacent) {
                if (adjacent == no_node || nodes[adjacent].cell.element != Element::signal) continue;
                ++inputs; if (previous[adjacent] != 0) ++active;
            }
        }
        materials[i] = material(element, is_communicator(element) ? received[i] : control(element, active, inputs));
        if (materials[i] == Material::source) { power_[i] = 15; frontier.emplace_back(i, std::uint8_t{15}); }
    }
    for (std::size_t head = 0; head < frontier.size(); ++head) {
        const auto [from, outgoing] = frontier[head];
        for (std::size_t direction = 0; direction < 4; ++direction) {
            if ((outgoing & (1U << direction)) == 0) continue;
            const auto next = nodes[from].adjacent[direction];
            if (next == no_node || materials[next] == Material::blocked || !connects(nodes[from].cell.element, nodes[next].cell.element)) continue;
            const auto channel = materials[next] == Material::crossing
                ? static_cast<std::uint8_t>(direction % 2 == 0 ? 5 : 10) : std::uint8_t{15};
            const auto added = static_cast<std::uint8_t>(channel & static_cast<std::uint8_t>(~power_[next]));
            if (added == 0) continue;
            power_[next] = static_cast<std::uint8_t>(power_[next] | added);
            frontier.emplace_back(next, added);
        }
    }
    valid_.assign(nodes.size(), true); snapshot_dirty_ = true;
    ++ticks_;
}

void Simulation::reset() {
    state_.clear();
    power_.assign(topology_.nodes().size(), 0);
    sent_.assign(power_.size(), false); valid_.assign(power_.size(), false);
    snapshot_dirty_ = false;
    ticks_ = 0;
}

void Simulation::invalidate(std::span<const Point> points) {
    for (const auto point : points) {
        const auto index = topology_.index(point);
        if (index != no_node) { power_[index] = 0; sent_[index] = false; valid_[index] = false; snapshot_dirty_ = true; }
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
