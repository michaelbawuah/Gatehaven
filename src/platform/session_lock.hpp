#pragma once
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <thread>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace gatehaven::platform {
[[noreturn]] inline void os_error(const char* operation) {
#ifdef _WIN32
    throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), operation);
#else
    throw std::system_error(errno, std::generic_category(), operation);
#endif
}

class FileLock {
public:
    explicit FileLock(const std::filesystem::path& path) {
#ifdef _WIN32
        handle_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) os_error("Open session lock");
#else
        handle_ = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (handle_ < 0) os_error("Open session lock");
#endif
    }
    ~FileLock() {
        unlock();
#ifdef _WIN32
        CloseHandle(handle_);
#else
        ::close(handle_);
#endif
    }
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;

    bool try_lock(bool exclusive) {
#ifdef _WIN32
        OVERLAPPED position{};
        const DWORD flags = LOCKFILE_FAIL_IMMEDIATELY | (exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0);
        if (!LockFileEx(handle_, flags, 0, 1, 0, &position)) {
            if (GetLastError() == ERROR_LOCK_VIOLATION) return false;
            os_error("Lock session");
        }
#else
        if (flock(handle_, (exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB) != 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) return false;
            os_error("Lock session");
        }
#endif
        locked_ = true;
        return true;
    }

    void lock(bool exclusive) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!try_lock(exclusive)) {
            if (std::chrono::steady_clock::now() >= deadline) {
                throw std::runtime_error("Session is busy; try again");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    void unlock() noexcept {
        if (!locked_) return;
#ifdef _WIN32
        OVERLAPPED position{};
        UnlockFileEx(handle_, 0, 1, 0, &position);
#else
        flock(handle_, LOCK_UN);
#endif
        locked_ = false;
    }
private:
#ifdef _WIN32
    HANDLE handle_{INVALID_HANDLE_VALUE};
#else
    int handle_{-1};
#endif
    bool locked_{};
};

inline std::filesystem::path prepare_private_directory(const std::filesystem::path& path) {
    std::filesystem::create_directories(path);
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(path))) {
        throw std::runtime_error("Session directory must not be a symbolic link");
    }
#ifndef _WIN32
    struct stat info{};
    if (lstat(path.c_str(), &info) != 0) os_error("Inspect clipboard directory");
    if (!S_ISDIR(info.st_mode) || info.st_uid != geteuid()) {
        throw std::runtime_error("Session directory must belong to this user");
    }
    if (chmod(path.c_str(), 0700) != 0) os_error("Protect clipboard directory");
#endif
    return path;
}

}
