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
    void hold_screen(Point point) { held_.insert(point); }
    void release_screens() { held_.clear(); }
    void reset_protocols();
    void prune(const Circuit& circuit);
    void clear() { bindings_.clear(); held_.clear(); }
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
    std::map<Point, Binding> bindings_;
    std::set<Point> held_;
    std::uint64_t generation_{};
};
}
