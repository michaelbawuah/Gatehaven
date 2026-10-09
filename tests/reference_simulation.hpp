#pragma once
// Slow Gatehaven traversal oracle. Source/Signal connectivity was corrected
// after external v0.4 observations; this is not the external reference engine.
// Map-based traversal oracle for differential checks of optimized simulation.
#include "gatehaven/simulation.hpp"

#include <array>
#include <utility>
#include <vector>

namespace reference {
using namespace gatehaven;
class ReferenceSimulation {
public:
    using Exchange = std::function<bool(const CommunicatorGroup&, bool)>;
    void step(const Circuit& circuit, const Exchange& exchange = {});
    void reset();
    void invalidate(std::span<const Point> points);
    [[nodiscard]] bool powered(Point point) const { return ports(point) != 0; }
    [[nodiscard]] std::uint8_t ports(Point point) const;
    [[nodiscard]] std::uint64_t ticks() const noexcept { return ticks_; }
    [[nodiscard]] std::size_t powered_count() const;
    [[nodiscard]] bool sent(Point point) const { const auto it = sent_.find(point); return it != sent_.end() && it->second; }
    [[nodiscard]] const std::map<Point, Power>& snapshot() const noexcept { return state_; }

private:
    std::map<Point, Power> state_;
    std::map<Point, bool> sent_;
    std::uint64_t ticks_{};
};

namespace {

enum class Material { blocked, conductor, crossing, source };
struct Node { Element element; Material material; std::uint8_t ports{}; };

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
    return element == Element::wire || element == Element::crossing || element == Element::signal || element == Element::source;
}

constexpr bool connects(Element a, Element b) {
    return (a != Element::signal || signal_connects(b)) &&
           (b != Element::signal || signal_connects(a));
}

constexpr std::uint8_t bit(Direction direction) {
    return static_cast<std::uint8_t>(1U << std::to_underlying(direction));
}

} // namespace

void ReferenceSimulation::step(const Circuit& circuit, const Exchange& exchange) {
    std::map<Point, Node> nodes;
    std::vector<std::pair<Point, std::uint8_t>> frontier;
    frontier.reserve(circuit.size());
    std::map<Point, bool> received;
    sent_.clear();
    for (const auto& group : communicator_groups(circuit)) {
        bool sending = false;
        for (const auto point : group.cells) {
            for (const auto direction : directions) {
                const auto next = neighbor(point, direction);
                if (!next || circuit.at(*next) != Element::signal) continue;
                const auto previous = state_.find(*next);
                if (previous != state_.end() && previous->second.element == Element::signal && previous->second.ports != 0) sending = true;
            }
        }
        const bool receiving = exchange && exchange(group, sending);
        for (const auto point : group.cells) { received[point] = receiving; sent_[point] = sending; }
    }

    // Read only state_ here. New power cannot affect controls in this step.
    for (const auto& cell : circuit.cells()) {
        unsigned input_count = 0;
        unsigned on_count = 0;
        if (cell.element >= Element::positive_relay && !is_communicator(cell.element)) for (const auto direction : directions) {
            const auto next = neighbor(cell.position, direction);
            if (!next || circuit.at(*next) != Element::signal) continue;
            ++input_count;
            const auto previous = state_.find(*next);
            if (previous != state_.end() && previous->second.element == Element::signal &&
                previous->second.ports != 0) ++on_count;
        }
        const auto active_material = material(cell.element, is_communicator(cell.element) ? received.at(cell.position) : control(cell.element, on_count, input_count));
        auto& node = nodes.emplace(cell.position, Node{cell.element, active_material, 0}).first->second;
        if (active_material == Material::source) {
            node.ports = 15;
            frontier.emplace_back(cell.position, std::uint8_t{15});
        }
    }

    for (std::size_t head = 0; head < frontier.size(); ++head) {
        const auto [point, outgoing] = frontier[head];
        const auto& from = nodes.at(point);
        for (const auto direction : directions) {
            if ((outgoing & bit(direction)) == 0) continue;
            const auto next = neighbor(point, direction);
            if (!next) continue;
            const auto found = nodes.find(*next);
            if (found == nodes.end()) continue;
            auto& to = found->second;
            if (to.material == Material::blocked || !connects(from.element, to.element)) continue;
            const std::uint8_t channel = to.material == Material::crossing
                ? static_cast<std::uint8_t>((direction == Direction::north || direction == Direction::south) ? 5 : 10)
                : std::uint8_t{15};
            const auto added = static_cast<std::uint8_t>(channel & static_cast<std::uint8_t>(~to.ports));
            if (added == 0) continue;
            to.ports = static_cast<std::uint8_t>(to.ports | added);
            frontier.emplace_back(*next, added);
        }
    }

    std::map<Point, Power> result;
    for (const auto& [point, node] : nodes) result.emplace(point, Power{node.element, node.ports});
    state_ = std::move(result);
    ++ticks_;
}

void ReferenceSimulation::reset() {
    state_.clear();
    sent_.clear();
    ticks_ = 0;
}

void ReferenceSimulation::invalidate(std::span<const Point> points) {
    for (const auto point : points) { state_.erase(point); sent_.erase(point); }
}

std::uint8_t ReferenceSimulation::ports(Point point) const {
    const auto found = state_.find(point);
    return found == state_.end() ? std::uint8_t{0} : found->second.ports;
}

std::size_t ReferenceSimulation::powered_count() const {
    std::size_t count = 0;
    for (const auto& [point, power] : state_) {
        static_cast<void>(point);
        if (power.ports != 0) ++count;
    }
    return count;
}

} // namespace reference
