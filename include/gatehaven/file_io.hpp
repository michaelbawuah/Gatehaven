#pragma once
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace gatehaven {
[[nodiscard]] std::expected<std::string, std::string> read_bounded_file(const std::filesystem::path& path, std::size_t limit);
[[nodiscard]] std::expected<void, std::string> replace_file(const std::filesystem::path& path, std::string_view bytes);
}
