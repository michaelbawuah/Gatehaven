#include "gatehaven/statistics.hpp"
#include <limits>

namespace gatehaven {
CircuitStatistics statistics(const Circuit& circuit) {
    CircuitStatistics result; result.cells = circuit.size(); result.area = 0;
    if (const auto bounds = circuit.bounds()) {
        result.width = static_cast<std::uint64_t>(static_cast<std::int64_t>(bounds->max.x) - bounds->min.x + 1);
        result.height = static_cast<std::uint64_t>(static_cast<std::int64_t>(bounds->max.y) - bounds->min.y + 1);
        if (result.width > std::numeric_limits<std::uint64_t>::max() / result.height) result.area.reset();
        else result.area = result.width * result.height;
    }
    for (std::size_t i = 0; i < result.elements.size(); ++i) result.elements[i] = circuit.count(static_cast<Element>(i));
    return result;
}
void write_statistics(std::ostream& out, const CircuitStatistics& result) {
    out << "{\"cells\":" << result.cells << ",\"width\":" << result.width << ",\"height\":" << result.height << ",\"area\":";
    if (result.area) out << *result.area; else out << "null";
    out << ",\"elements\":{";
    for (std::size_t i = 1; i < result.elements.size(); ++i) {
        if (i != 1) out << ',';
        out << '"' << element_names[i] << "\":" << result.elements[i];
    }
    out << "}}\n";
}
}
