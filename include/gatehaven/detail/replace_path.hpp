#pragma once
#include <filesystem>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <string>
#include <system_error>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace gatehaven::detail {
inline std::string temporary_token() {
    static std::atomic<std::uint64_t> sequence{};
#ifdef _WIN32
    const auto process = GetCurrentProcessId();
#else
    const auto process = getpid();
#endif
    return std::to_string(process) + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
        "-" + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
}

// The caller has already flushed and closed its unique temporary file.
// A competing Windows replacement can briefly retain the target's delete lock.
inline std::error_code replace_path(const std::filesystem::path& temporary, const std::filesystem::path& destination) {
#ifdef _WIN32
    DWORD failure{};
    for (unsigned attempt = 0; attempt < 40; ++attempt) {
        if (MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return {};
        failure = GetLastError();
        if (failure != ERROR_SHARING_VIOLATION && failure != ERROR_LOCK_VIOLATION && failure != ERROR_ACCESS_DENIED) break;
        if (attempt != 39) Sleep(5);
    }
    return {static_cast<int>(failure), std::system_category()};
#else
    std::error_code failure;
    std::filesystem::rename(temporary, destination, failure);
    return failure;
#endif
}
}
