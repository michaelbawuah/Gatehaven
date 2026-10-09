#pragma once
#include "gatehaven/communicator.hpp"
#include "gatehaven/serial_protocol.hpp"
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

namespace gatehaven {
class FileEndpoints {
public:
    [[nodiscard]] std::expected<void, std::string> choose_input(Point anchor, const std::filesystem::path& path);
    [[nodiscard]] std::expected<void, std::string> choose_output(Point anchor, const std::filesystem::path& path);
    [[nodiscard]] bool exchange(const CommunicatorGroup& group, bool sending);
    // The revision identifies immutable group membership, as supplied by Circuit.
    [[nodiscard]] bool exchange(const CommunicatorGroup& group, bool sending, std::uint64_t circuit_revision);
    [[nodiscard]] const std::string& last_error() const { return last_error_; }
    void hold_screen(Point point) { if (held_.insert(point).second) invalidate_routes(); }
    void release_screens() { if (!held_.empty()) { held_.clear(); invalidate_routes(); } }
    void reset_protocols();
    void prune(const Circuit& circuit);
    void clear() { invalidate_routes(); bindings_.clear(); held_.clear(); last_error_.clear(); }
    [[nodiscard]] std::string error(Point anchor) const;
    [[nodiscard]] std::size_t bound_files() const;
private:
    struct Binding {
        Element element{Element::empty};
        std::ifstream input;
        std::ofstream output;
        InputProtocol input_protocol;
        OutputProtocol output_protocol;
        std::uint64_t generation{};
        std::vector<Point> group;
    };
    struct Route { Binding* binding{}; bool screen{}; std::uint64_t revision{}; };
    [[nodiscard]] Binding& resolve(const CommunicatorGroup& group);
    [[nodiscard]] bool exchange(Binding& binding, bool sending);
    void invalidate_routes() { routes_.clear(); pruned_revision_.reset(); }
    std::map<Point, Route> routes_;
    std::optional<std::uint64_t> pruned_revision_;
    std::string last_error_;
    std::map<Point, Binding> bindings_;
    std::set<Point> held_;
    std::uint64_t generation_{};
};
}
