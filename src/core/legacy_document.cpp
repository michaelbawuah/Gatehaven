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
    std::array<char, 65536> buffer{};
    for (std::uint64_t offset = 0; offset < area;) {
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), area - offset));
        if (!input.read(buffer.data(), static_cast<std::streamsize>(count)))
            return std::unexpected(failure(input.eof() ? "Truncated legacy cell data" : "Could not read legacy cell data"));
        for (std::size_t i = 0; i < count; ++i) {
            const auto byte = static_cast<unsigned char>(buffer[i]);
            const auto kind = static_cast<std::size_t>(byte >> 2);
            const auto index = offset + i;
            if (kind >= legacy_elements.size()) return std::unexpected(failure("Unknown legacy element at byte " + std::to_string(16 + index)));
            if (kind == 0) continue;
            if (circuit.size() >= limits.max_cells) return std::unexpected(failure("Legacy document exceeds occupied-cell limit"));
            circuit.set({static_cast<Coordinate>(index % width), static_cast<Coordinate>(index / width)},
                        legacy_elements[kind], static_cast<std::uint8_t>(byte & 3));
        }
        offset += count;
    }
    // The reference reader ignores trailing bytes. Keep that behavior for imports;
    // canonical exports contain exactly one header and its rectangular payload.
    return circuit;
}
}

namespace gatehaven {
std::expected<LegacyLayout, DocumentError> legacy_layout(const Circuit& circuit, DocumentLimits limits) {
    const auto bounds = circuit.bounds();
    if (!bounds) return LegacyLayout{};
    const auto width = static_cast<std::uint64_t>(static_cast<std::int64_t>(bounds->max.x) - bounds->min.x + 1);
    const auto height = static_cast<std::uint64_t>(static_cast<std::int64_t>(bounds->max.y) - bounds->min.y + 1);
    constexpr auto maximum = static_cast<std::uint64_t>(std::numeric_limits<Coordinate>::max());
    // Division avoids overflow even across the full signed coordinate range.
    if (width > maximum || height > maximum || width > maximum / height)
        return std::unexpected(failure("Circuit rectangle cannot be represented in legacy format"));
    if (width * height > limits.max_legacy_area)
        return std::unexpected(failure("Legacy export exceeds area limit; use a native .ghv document"));
    if (circuit.size() > limits.max_cells) return std::unexpected(failure("Legacy export exceeds occupied-cell limit"));
    return LegacyLayout{*bounds, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
}
std::expected<void, DocumentError> write_legacy_document(std::ostream& output, const Circuit& circuit, DocumentLimits limits) {
    const auto layout = legacy_layout(circuit, limits);
    if (!layout) return std::unexpected(layout.error());
    output.write("CCPG", 4);
    for (const auto value : {0U, layout->width, layout->height}) for (unsigned byte = 0; byte < 4; ++byte)
        output.put(static_cast<char>((value >> (byte * 8)) & 255));
    std::array<char, 65536> buffer{};
    std::size_t used = 0;
    std::uint64_t cursor = 0;
    const auto flush = [&] {
        output.write(buffer.data(), static_cast<std::streamsize>(used));
        used = 0; buffer.fill(0);
    };
    const auto zeros = [&](std::uint64_t count) {
        while (count && output) {
            const auto chunk = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size() - used, count));
            used += chunk; count -= chunk;
            if (used == buffer.size()) flush();
        }
    };
    if (!circuit.empty()) circuit.visit(layout->bounds, [&](const Cell& cell) {
        if (!output) return;
        const auto x = static_cast<std::uint64_t>(static_cast<std::int64_t>(cell.position.x) - layout->bounds.min.x);
        const auto y = static_cast<std::uint64_t>(static_cast<std::int64_t>(cell.position.y) - layout->bounds.min.y);
        const auto position = y * layout->width + x;
        zeros(position - cursor);
        const auto id = static_cast<unsigned>(std::find(legacy_elements.begin(), legacy_elements.end(), cell.element) - legacy_elements.begin());
        buffer[used++] = static_cast<char>((id << 2) | cell.state);
        if (used == buffer.size()) flush();
        cursor = position + 1;
    });
    zeros(layout->area() - cursor);
    if (used) flush();
    if (!output) return std::unexpected(failure("Could not write legacy document"));
    return {};
}
}
