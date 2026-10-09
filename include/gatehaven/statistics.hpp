#pragma once
#include "gatehaven/circuit.hpp"
#include <ostream>

namespace gatehaven {
struct CircuitStatistics {
    std::size_t cells{};
    std::uint64_t width{}, height{};
    std::optional<std::uint64_t> area;
    std::array<std::size_t, element_names.size()> elements{};
};
[[nodiscard]] CircuitStatistics statistics(const Circuit& circuit);
void write_statistics(std::ostream& out, const CircuitStatistics& statistics);
}
