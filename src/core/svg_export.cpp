#include "gatehaven/svg_export.hpp"

namespace gatehaven {
std::expected<void, std::string> export_svg(std::ostream& out, const Circuit& circuit, const Simulation* state) {
    if (circuit.size() > 100000) return std::unexpected("SVG export is limited to 100000 occupied cells");
    const auto bounds = circuit.bounds().value_or(Bounds{{0, 0}, {0, 0}});
    const auto width = static_cast<std::int64_t>(bounds.max.x) - bounds.min.x + 3;
    const auto height = static_cast<std::int64_t>(bounds.max.y) - bounds.min.y + 3;
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" role=\"img\" viewBox=\""
        << static_cast<std::int64_t>(bounds.min.x) - 1 << ' ' << static_cast<std::int64_t>(bounds.min.y) - 1 << ' '
        << width << ' ' << height << "\"><title>Gatehaven circuit</title>\n";
    out << "<rect x=\"" << static_cast<std::int64_t>(bounds.min.x) - 1 << "\" y=\"" << static_cast<std::int64_t>(bounds.min.y) - 1
        << "\" width=\"" << width << "\" height=\"" << height << "\" fill=\"#f6f8f8\"/>\n";
    for (const auto& cell : circuit.cells()) {
        const auto color = state && state->powered(cell.position) ? "#00856f" : cell.element == Element::source ? "#dc6228" : "#5b5982";
        out << "<g transform=\"translate(" << cell.position.x << ' ' << cell.position.y << ")\"><title>"
            << name(cell.element) << " at " << cell.position.x << ',' << cell.position.y << "</title>"
            << "<rect x=\".05\" y=\".05\" width=\".9\" height=\".9\" rx=\".06\" fill=\"" << color << "\"/>"
            << "<text x=\".5\" y=\".56\" text-anchor=\"middle\" fill=\"white\" font-family=\"monospace\" font-size=\".13\">"
            << name(cell.element) << "</text></g>\n";
    }
    out << "</svg>\n";
    if (!out) return std::unexpected("Could not write SVG");
    return {};
}
}
