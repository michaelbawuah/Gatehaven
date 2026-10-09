#include "gatehaven/clipboard_codec.hpp"

#include <set>
#include <algorithm>

namespace gatehaven {
namespace {
constexpr std::string_view magic = "GHCLIP01";
constexpr std::string_view state_magic = "GHCLIP02";
constexpr std::uint64_t max_extent = std::uint64_t{1} << 32;

void append(std::string& out, std::uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        out.push_back(static_cast<char>((value >> (i * 8)) & 255));
    }
}

std::uint64_t integer(std::string_view bytes, std::size_t offset, unsigned count) {
    std::uint64_t result = 0;
    for (unsigned i = 0; i < count; ++i) {
        result |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[offset + i])) << (i * 8);
    }
    return result;
}

std::uint64_t checksum(std::string_view bytes) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char byte : bytes) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool dimensions(std::uint64_t width, std::uint64_t height, std::uint64_t count) {
    if (width > max_extent || height > max_extent || count > max_clipboard_cells) return false;
    if (width == 0 || height == 0) return width == 0 && height == 0 && count == 0;
    return true;
}

bool valid_cell(std::uint64_t x, std::uint64_t y, std::uint64_t width,
                std::uint64_t height, std::uint64_t element) {
    return x < width && y < height && element > 0 && element < element_names.size();
}
} // namespace

std::expected<std::string, std::string> encode_stamp(const Stamp& stamp) {
    const auto width = static_cast<std::uint64_t>(stamp.width);
    const auto height = static_cast<std::uint64_t>(stamp.height);
    if (!dimensions(width, height, stamp.cells.size())) return std::unexpected("Invalid clipboard dimensions or size");
    std::set<std::pair<std::int64_t, std::int64_t>> seen;
    const bool with_state = std::any_of(stamp.cells.begin(), stamp.cells.end(), [](const auto& cell) { return cell.state != 0; });
    std::string bytes(with_state ? state_magic : magic);
    bytes.reserve(40 + 9 * stamp.cells.size());
    append(bytes, width, 8);
    append(bytes, height, 8);
    append(bytes, stamp.cells.size(), 8);
    for (const auto& cell : stamp.cells) {
        if (!valid_cell(static_cast<std::uint64_t>(cell.x), static_cast<std::uint64_t>(cell.y),
                        width, height, static_cast<std::uint64_t>(cell.element)) ||
            cell.state > 3 || !seen.emplace(cell.x, cell.y).second) return std::unexpected("Invalid or duplicate clipboard cell");
        append(bytes, static_cast<std::uint64_t>(cell.x), 4);
        append(bytes, static_cast<std::uint64_t>(cell.y), 4);
        append(bytes, static_cast<std::uint64_t>(cell.element) | (static_cast<std::uint64_t>(cell.state) << 4), 1);
    }
    append(bytes, checksum(bytes), 8);
    return bytes;
}

std::expected<Stamp, std::string> decode_stamp(std::string_view bytes) {
    if (bytes.size() < 40 || bytes.size() > max_clipboard_bytes || (!bytes.starts_with(magic) && !bytes.starts_with(state_magic))) {
        return std::unexpected("Invalid clipboard header or size");
    }
    const auto width = integer(bytes, 8, 8);
    const auto height = integer(bytes, 16, 8);
    const auto count = integer(bytes, 24, 8);
    if (!dimensions(width, height, count) || bytes.size() != 40 + count * 9) {
        return std::unexpected("Invalid clipboard dimensions or length");
    }
    if (integer(bytes, bytes.size() - 8, 8) != checksum(bytes.substr(0, bytes.size() - 8))) {
        return std::unexpected("Clipboard checksum mismatch");
    }
    Stamp stamp{static_cast<std::int64_t>(width), static_cast<std::int64_t>(height), {}};
    stamp.cells.reserve(static_cast<std::size_t>(count));
    std::set<std::pair<std::uint64_t, std::uint64_t>> seen;
    for (std::size_t i = 0; i < count; ++i) {
        const auto offset = 32 + i * 9;
        const auto x = integer(bytes, offset, 4);
        const auto y = integer(bytes, offset + 4, 4);
        const auto encoded = integer(bytes, offset + 8, 1);
        const auto element = bytes.starts_with(state_magic) ? encoded & 15 : encoded;
        const auto state = bytes.starts_with(state_magic) ? encoded >> 4 : 0;
        if (!valid_cell(x, y, width, height, element) || state > 3 || !seen.emplace(x, y).second) {
            return std::unexpected("Invalid or duplicate clipboard cell");
        }
        stamp.cells.push_back({static_cast<std::int64_t>(x), static_cast<std::int64_t>(y),
                               static_cast<Element>(element), static_cast<std::uint8_t>(state)});
    }
    return stamp;
}
} // namespace gatehaven
