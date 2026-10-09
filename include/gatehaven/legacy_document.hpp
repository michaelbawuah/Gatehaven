#pragma once
#include "gatehaven/document.hpp"

namespace gatehaven {
// File identifiers are deliberately independent from Element's numeric values.
inline constexpr std::array legacy_elements{
    Element::empty, Element::wire, Element::crossing, Element::signal, Element::source,
    Element::positive_relay, Element::negative_relay, Element::and_gate, Element::or_gate,
    Element::nand_gate, Element::nor_gate, Element::screen, Element::file_input, Element::file_output};

struct LegacyLayout {
    Bounds bounds{};
    std::uint32_t width{};
    std::uint32_t height{};
    [[nodiscard]] std::uint64_t area() const { return static_cast<std::uint64_t>(width) * height; }
};
[[nodiscard]] std::expected<Circuit, DocumentError> read_legacy_document(std::istream& input, DocumentLimits limits = {});
[[nodiscard]] std::expected<LegacyLayout, DocumentError> legacy_layout(const Circuit& circuit, DocumentLimits limits = {});
[[nodiscard]] std::expected<void, DocumentError> write_legacy_document(std::ostream& output, const Circuit& circuit, DocumentLimits limits = {});
}
