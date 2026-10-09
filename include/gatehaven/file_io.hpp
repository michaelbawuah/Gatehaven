#pragma once
#include <expected>
#include <cstdint>
#include <optional>
#include <filesystem>
#include <string>
#include <string_view>

namespace gatehaven {
[[nodiscard]] std::expected<std::string, std::string> read_bounded_file(const std::filesystem::path& path, std::size_t limit);
[[nodiscard]] std::expected<void, std::string> replace_file(const std::filesystem::path& path, std::string_view bytes);
}

namespace gatehaven {
// Change detection only; this noncryptographic digest is not an authenticity check.
struct FileFingerprint {
    std::uintmax_t bytes{};
    std::filesystem::file_time_type modified{};
    std::uint64_t digest{};
    bool operator==(const FileFingerprint&) const = default;
};
[[nodiscard]] std::expected<std::optional<FileFingerprint>, std::string> fingerprint_file(
    const std::filesystem::path& path, std::uintmax_t limit = 384'000'000);
}
