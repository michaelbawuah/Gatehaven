#include "gatehaven/file_io.hpp"
#include "gatehaven/detail/replace_path.hpp"
#include <chrono>
#include <array>
#include <fstream>
#include <limits>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace gatehaven {
std::expected<std::string, std::string> read_bounded_file(const std::filesystem::path& path, std::size_t limit) {
    try {
        const auto size = std::filesystem::file_size(path);
        if (size > limit || size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) return std::unexpected("File exceeds size limit");
        std::ifstream in(path, std::ios::binary);
        std::string bytes(static_cast<std::size_t>(size), '\0');
        if (!in.read(bytes.data(), static_cast<std::streamsize>(size)) || in.peek() != EOF) return std::unexpected("Could not read file");
        return bytes;
    } catch (const std::exception& e) { return std::unexpected(e.what()); }
}
std::expected<void, std::string> replace_file(const std::filesystem::path& path, std::string_view bytes) {
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) return std::unexpected("File exceeds stream size limit");
    const auto token = detail::temporary_token();
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        auto temporary = path;
        temporary += ".tmp-" + token + "-" + std::to_string(attempt);
        std::ofstream out(temporary, std::ios::binary | std::ios::noreplace);
        if (!out) continue;
        struct Cleanup {
            std::filesystem::path path;
            std::ofstream& stream;
            ~Cleanup() { if (stream.is_open()) stream.close(); std::error_code error; std::filesystem::remove(path, error); }
        } cleanup{temporary, out};
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size())); out.flush();
        if (!out) return std::unexpected("Could not write temporary file");
        out.close();
        if (!out) return std::unexpected("Could not close temporary file");
        if (const auto error = detail::replace_path(temporary, path)) return std::unexpected(error.message());
        return {};
    }
    return std::unexpected("Could not create a temporary file beside the destination");
}
}

namespace gatehaven {
std::expected<std::optional<FileFingerprint>, std::string> fingerprint_file(
    const std::filesystem::path& path, std::uintmax_t limit) {
    try {
        std::error_code error;
        const auto state = std::filesystem::status(path, error);
        if (error == std::errc::no_such_file_or_directory || (!error && !std::filesystem::exists(state))) return std::optional<FileFingerprint>{};
        if (error) return std::unexpected(error.message());
        if (!std::filesystem::is_regular_file(state)) return std::unexpected("Document is not a regular file");
        FileFingerprint result{std::filesystem::file_size(path), std::filesystem::last_write_time(path), 14695981039346656037ULL};
        if (result.bytes > limit) return std::unexpected("Document exceeds inspection limit");
        std::ifstream input(path, std::ios::binary);
        if (!input) return std::unexpected("Could not inspect document");
        std::array<char, 65536> buffer{};
        std::uintmax_t count = 0;
        while (input) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto size = input.gcount();
            count += static_cast<std::uintmax_t>(size);
            if (count > limit) return std::unexpected("Document grew beyond inspection limit");
            for (std::streamsize i = 0; i < size; ++i) { result.digest ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]); result.digest *= 1099511628211ULL; }
        }
        if (!input.eof() || input.bad()) return std::unexpected("Could not finish inspecting document");
        if (count != result.bytes || std::filesystem::file_size(path) != result.bytes ||
            std::filesystem::last_write_time(path) != result.modified) return std::unexpected("Document changed while being inspected");
        return std::optional{result};
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
}
