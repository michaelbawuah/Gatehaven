#include "gatehaven/file_io.hpp"
#include "gatehaven/detail/replace_path.hpp"
#include <chrono>
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
