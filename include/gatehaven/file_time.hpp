#pragma once
#include <chrono>
#include <filesystem>

namespace gatehaven {
inline auto system_time(std::filesystem::file_time_type time) {
#ifdef _MSC_VER
    // MSVC's file_clock uses the UTC conversion pair permitted by C++20.
    return std::chrono::clock_cast<std::chrono::system_clock>(time);
#else
    return std::chrono::file_clock::to_sys(time);
#endif
}
inline std::filesystem::file_time_type file_time(std::chrono::sys_seconds time) {
#ifdef _MSC_VER
    return std::chrono::clock_cast<std::chrono::file_clock>(time);
#else
    return std::chrono::file_clock::from_sys(time);
#endif
}
}
