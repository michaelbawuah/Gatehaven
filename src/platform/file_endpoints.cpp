#include "gatehaven/file_endpoints.hpp"
#include <algorithm>

namespace gatehaven {
std::expected<void, std::string> FileEndpoints::choose_input(Point anchor, const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::unexpected("Could not open the chosen input file");
    auto& binding = bindings_[anchor];
    if (binding.element != Element::file_input) binding = {};
    binding.element = Element::file_input; binding.input = std::move(input);
    binding.generation = ++generation_; binding.input_protocol.retry();
    return {};
}
std::expected<void, std::string> FileEndpoints::choose_output(Point anchor, const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return std::unexpected("Could not open the chosen output file");
    auto& binding = bindings_[anchor];
    if (binding.element != Element::file_output) binding = {};
    binding.element = Element::file_output; binding.output = std::move(output);
    binding.generation = ++generation_; binding.output_protocol.retry();
    return {};
}

bool FileEndpoints::exchange(const CommunicatorGroup& group, bool sending) {
    if (group.element == Element::screen) {
        return std::any_of(group.cells.begin(), group.cells.end(), [&](Point p) { return held_.contains(p); });
    }
    auto* binding = &bindings_[group.id];
    for (const auto point : group.cells) {
        const auto found = bindings_.find(point);
        if (found != bindings_.end() && found->second.element == group.element && found->second.generation > binding->generation) binding = &found->second;
    }
    if (binding->element != group.element) { *binding = {}; binding->element = group.element; }
    if (!binding->group.empty() && binding->group != group.cells) {
        binding->input_protocol.reset(); binding->output_protocol.reset();
    }
    binding->group = group.cells;
    if (group.element == Element::file_input) {
        const InputProtocol::Read read = [&]() -> std::expected<std::optional<std::uint8_t>, std::string> {
            if (!binding->input.is_open()) return std::optional<std::uint8_t>{};
            const auto byte = binding->input.get();
            if (binding->input.bad()) return std::unexpected("Input file read failed");
            if (byte == EOF) return std::optional<std::uint8_t>{};
            return std::optional<std::uint8_t>{static_cast<std::uint8_t>(byte)};
        };
        const InputProtocol::More more = [&]() -> std::expected<bool, std::string> {
            if (!binding->input.is_open()) return false;
            const auto byte = binding->input.peek();
            if (binding->input.bad()) return std::unexpected("Input file query failed");
            return byte != EOF;
        };
        return binding->input_protocol.step(sending, read, more);
    }
    const OutputProtocol::Write write = [&](std::uint8_t byte) -> std::expected<void, std::string> {
        if (!binding->output.is_open()) return std::unexpected("Choose an output file with Interact");
        binding->output.put(static_cast<char>(byte)); binding->output.flush();
        if (!binding->output) return std::unexpected("Output file write failed");
        return {};
    };
    return binding->output_protocol.step(sending, write);
}
void FileEndpoints::reset_protocols() {
    release_screens();
    for (auto& [point, binding] : bindings_) {
        static_cast<void>(point); binding.input_protocol.reset(); binding.output_protocol.reset();
    }
}
void FileEndpoints::prune(const Circuit& circuit) {
    std::erase_if(bindings_, [&](const auto& entry) { return circuit.at(entry.first) != entry.second.element; });
    std::erase_if(held_, [&](Point p) { return circuit.at(p) != Element::screen; });
}
std::string FileEndpoints::error(Point anchor) const {
    const auto it = bindings_.find(anchor);
    if (it == bindings_.end()) return {};
    return it->second.element == Element::file_input ? it->second.input_protocol.error() : it->second.output_protocol.error();
}
std::size_t FileEndpoints::bound_files() const {
    return static_cast<std::size_t>(std::count_if(bindings_.begin(), bindings_.end(), [](const auto& entry) { return entry.second.generation != 0; }));
}
}
