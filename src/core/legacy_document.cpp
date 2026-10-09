#include "gatehaven/legacy_document.hpp"
#include <algorithm>
#include <istream>
#include <ostream>

namespace gatehaven {
namespace {
std::uint32_t little(const std::array<unsigned char, 16>& header, std::size_t offset) {
    std::uint32_t value = 0;
    for (unsigned byte = 0; byte < 4; ++byte) value |= static_cast<std::uint32_t>(header[offset + byte]) << (byte * 8);
    return value;
}
DocumentError failure(std::string message) { return {0, std::move(message)}; }
}

std::expected<Circuit, DocumentError> read_legacy_document(std::istream& input, DocumentLimits limits) {
    std::array<unsigned char, 16> header{};
    if (!input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size())))
        return std::unexpected(failure(input.eof() ? "Truncated legacy header" : "Could not read legacy header"));
    if (std::string_view(reinterpret_cast<const char*>(header.data()), 4) != "CCPG")
        return std::unexpected(failure("Expected CCPG legacy header"));
    if (little(header, 4) != 0) return std::unexpected(failure("Unsupported legacy version"));
    const auto width = little(header, 8), height = little(header, 12);
    const auto area = static_cast<std::uint64_t>(width) * height;
    constexpr auto maximum = static_cast<std::uint32_t>(std::numeric_limits<Coordinate>::max());
    if (width > maximum || height > maximum || area > maximum)
        return std::unexpected(failure("Invalid legacy dimensions"));
    if (area > limits.max_legacy_area) return std::unexpected(failure("Legacy rectangle exceeds area limit"));
    Circuit circuit;
    for (std::uint64_t index = 0; index < area; ++index) {
        char raw{};
        if (!input.get(raw)) return std::unexpected(failure(input.eof() ? "Truncated legacy cell data" : "Could not read legacy cell data"));
        const auto byte = static_cast<unsigned char>(raw);
        const auto kind = static_cast<std::size_t>(byte >> 2);
        if (kind >= legacy_elements.size()) return std::unexpected(failure("Unknown legacy element at byte " + std::to_string(16 + index)));
        if (kind == 0) continue;
        if (circuit.size() >= limits.max_cells) return std::unexpected(failure("Legacy document exceeds occupied-cell limit"));
        circuit.set({static_cast<Coordinate>(index % width), static_cast<Coordinate>(index / width)},
                    legacy_elements[kind], static_cast<std::uint8_t>(byte & 3));
    }
    // The reference reader ignores trailing bytes. Keep that behavior for imports;
    // canonical exports contain exactly one header and its rectangular payload.
    return circuit;
}
}
