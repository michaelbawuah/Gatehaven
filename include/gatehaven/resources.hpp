#pragma once
#include "gatehaven/paths.hpp"
#include <expected>
#include <optional>
namespace gatehaven {
[[nodiscard]] inline std::expected<std::string, std::string> file_uri(const std::filesystem::path& path) {
    std::error_code error;
    const auto absolute = std::filesystem::absolute(path, error);
    if (error) return std::unexpected(error.message());
    const auto bytes = absolute.generic_u8string();
    std::string result = bytes.starts_with(u8"//") ? "file:" : bytes.starts_with(u8"/") ? "file://" : "file:///";
    constexpr char hex[] = "0123456789ABCDEF";
    for (const auto byte : bytes) {
        const auto c = static_cast<unsigned char>(byte);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '/' || c == ':' || c == '-' || c == '_' || c == '.' || c == '~') result.push_back(static_cast<char>(c));
        else { result.push_back('%'); result.push_back(hex[c >> 4]); result.push_back(hex[c & 15]); }
    }
    return result;
}
[[nodiscard]] inline std::optional<std::filesystem::path> manual_path(const std::filesystem::path& executable) {
    const auto directory = executable.parent_path();
    for (const auto& candidate : {directory / "../Resources/docs/manual.html", directory / "../share/gatehaven/docs/manual.html",
                                  directory / "manual/manual.html", directory / "../manual/manual.html"}) {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error) && !error) return candidate.lexically_normal();
    }
    return std::nullopt;
}
}
