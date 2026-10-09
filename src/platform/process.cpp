#include "gatehaven/process.hpp"

#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace gatehaven {
std::expected<std::filesystem::path, std::string> current_executable() {
    try {
#ifdef _WIN32
        std::vector<wchar_t> buffer(512);
        while (buffer.size() <= 32768) {
            const auto size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (size == 0) return std::unexpected("Could not locate Gatehaven executable");
            if (size < buffer.size()) return std::filesystem::path(std::wstring(buffer.data(), size));
            buffer.resize(buffer.size() * 2);
        }
#elif defined(__APPLE__)
        std::uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::vector<char> buffer(size);
        if (_NSGetExecutablePath(buffer.data(), &size) == 0) return std::filesystem::canonical(buffer.data());
#else
        std::vector<char> buffer(512);
        while (buffer.size() <= 1024 * 1024) {
            const auto size = readlink("/proc/self/exe", buffer.data(), buffer.size());
            if (size < 0) return std::unexpected("Could not locate Gatehaven executable");
            if (static_cast<std::size_t>(size) < buffer.size()) {
                return std::filesystem::path(std::string(buffer.data(), static_cast<std::size_t>(size)));
            }
            buffer.resize(buffer.size() * 2);
        }
#endif
        return std::unexpected("Gatehaven executable path is unavailable or too long");
    } catch (const std::exception& e) { return std::unexpected(e.what()); }
}
}
