#pragma once

#include "gatehaven/circuit.hpp"
#include "gatehaven/topology.hpp"
#include "gatehaven/netlist.hpp"

#include <cstdint>
#include <map>
#include <span>
#include <functional>

namespace gatehaven {

struct Power {
    Element element{Element::empty};
    // N=1, E=2, S=4, W=8. Crossing wires may power just one axis.
    std::uint8_t ports{};
    bool operator==(const Power&) const = default;
};

struct SimulationMetrics {
    std::uint64_t topology_builds{};
    std::uint64_t propagations{};
    std::uint64_t frontier_visits{};
    std::uint64_t settled_ticks{};
};

class Simulation {
public:
    using Exchange = std::function<bool(const CommunicatorGroup&, bool)>;
    // Establish tick-zero power from saved levels, or reset levels when requested.
    void initialize(const Circuit& circuit, bool reset_levels = false);
    void refresh(const Circuit& circuit);
    void step(const Circuit& circuit, const Exchange& exchange = {});
    [[nodiscard]] bool conductive(Point point) const;
    [[nodiscard]] Circuit document_snapshot(const Circuit& circuit) const;
    void reset();
    [[nodiscard]] const SimulationMetrics& metrics() const noexcept { return metrics_; }
    void invalidate(std::span<const Point> points);
    [[nodiscard]] bool powered(Point point) const { return ports(point) != 0; }
    [[nodiscard]] std::uint8_t ports(Point point) const;
    [[nodiscard]] std::uint64_t ticks() const noexcept { return ticks_; }
    [[nodiscard]] std::size_t powered_count() const;
    [[nodiscard]] bool sent(Point point) const;
    [[nodiscard]] bool received(Point point) const;
    [[nodiscard]] const std::map<Point, Power>& snapshot() const;
    // Ordered by y then x. Visitors must not modify this simulation.
    template<class Visitor> void visit_state(Visitor&& visitor) const {
        const auto& nodes = topology_.nodes();
        for (std::size_t i = 0; i < nodes.size(); ++i) if (valid_[i])
            visitor(nodes[i].cell.position, Power{nodes[i].cell.element, node_power(i)}, group_flag(i, sent_), group_flag(i, received_));
    }

private:
    void rebuild(const Circuit& circuit);
    void energize(std::size_t vertex);
    void propagate();
    [[nodiscard]] std::uint8_t node_power(std::size_t node) const;
    [[nodiscard]] bool group_flag(std::size_t node, const std::vector<bool>& flags) const;
    std::uint64_t topology_revision_{std::numeric_limits<std::uint64_t>::max()};
    CompiledCircuit topology_;
    Netlist nets_;
    std::vector<std::uint8_t> power_;
    std::vector<std::uint8_t> previous_;
    std::vector<std::uint8_t> enabled_;
    std::vector<bool> received_;
    std::vector<std::size_t> frontier_;
    std::vector<bool> valid_;
    std::vector<bool> sent_;
    mutable std::map<Point, Power> state_;
    mutable bool snapshot_dirty_{};
    std::uint64_t ticks_{};
    SimulationMetrics metrics_;
    bool settled_{};
    bool invalidated_{};
};

} // namespace gatehaven
