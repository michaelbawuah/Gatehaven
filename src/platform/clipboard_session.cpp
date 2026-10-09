#include "gatehaven/clipboard_session.hpp"
#include "gatehaven/clipboard_codec.hpp"

#include <chrono>
#include <fstream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace gatehaven {
namespace {
[[noreturn]] void os_error(const char* operation) {
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
        if (handle_ == INVALID_HANDLE_VALUE) os_error("Open clipboard lock");
#else
        handle_ = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (handle_ < 0) os_error("Open clipboard lock");
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
            os_error("Lock clipboard");
        }
#else
        if (flock(handle_, (exclusive ? LOCK_EX : LOCK_SH) | LOCK_NB) != 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) return false;
            os_error("Lock clipboard");
        }
#endif
        locked_ = true;
        return true;
    }

    void lock(bool exclusive) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!try_lock(exclusive)) {
            if (std::chrono::steady_clock::now() >= deadline) {
                throw std::runtime_error("Clipboard is busy; try again");
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

struct Unlock {
    FileLock& lock;
    ~Unlock() { lock.unlock(); }
};

std::filesystem::path prepare_directory(const std::filesystem::path& path) {
    std::filesystem::create_directories(path);
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(path))) {
        throw std::runtime_error("Clipboard directory must not be a symbolic link");
    }
#ifndef _WIN32
    struct stat info{};
    if (lstat(path.c_str(), &info) != 0) os_error("Inspect clipboard directory");
    if (!S_ISDIR(info.st_mode) || info.st_uid != geteuid()) {
        throw std::runtime_error("Clipboard directory must belong to this user");
    }
    if (chmod(path.c_str(), 0700) != 0) os_error("Protect clipboard directory");
#endif
    return path;
}

std::optional<std::string> read_file(const std::filesystem::path& path, std::size_t limit) {
    const auto status = std::filesystem::symlink_status(path);
    if (!std::filesystem::exists(status)) return std::nullopt;
    if (!std::filesystem::is_regular_file(status)) throw std::runtime_error("Invalid clipboard file type");
    const auto length = std::filesystem::file_size(path);
    if (length > limit) throw std::runtime_error("Clipboard file exceeds size limit");
    std::ifstream input(path, std::ios::binary);
    std::string bytes(static_cast<std::size_t>(length), '\0');
    if (!input.read(bytes.data(), static_cast<std::streamsize>(length)) || input.peek() != EOF) {
        throw std::runtime_error("Could not read clipboard file");
    }
    return bytes;
}

void atomic_write(const std::filesystem::path& path, std::string_view bytes) {
    const auto temporary = path.parent_path() / "pending.tmp";
    std::filesystem::remove(temporary); // Serialized by transaction.lock, including crash recovery.
    struct Cleanup {
        const std::filesystem::path& path;
        ~Cleanup() { std::error_code error; std::filesystem::remove(path, error); }
    } cleanup{temporary};
    std::ofstream out(temporary, std::ios::binary | std::ios::noreplace);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.flush();
    if (!out) throw std::runtime_error("Could not write clipboard file");
    out.close();
    if (!out) throw std::runtime_error("Could not close clipboard file");
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        os_error("Replace clipboard file");
    }
#else
    std::filesystem::rename(temporary, path);
#endif
}
} // namespace

struct ClipboardSession::Impl {
    std::filesystem::path root;
    FileLock transaction;
    FileLock members;
    std::mutex mutex;
    bool joined{};

    explicit Impl(const std::filesystem::path& directory)
        : root(prepare_directory(directory)), transaction(root / "transaction.lock"), members(root / "members.lock") {
        transaction.lock(true);
        const Unlock release{transaction};
        if (members.try_lock(true)) {
            clear(); // No live members, including when the previous session crashed.
            members.unlock();
        }
        members.lock(false);
        joined = true;
    }

    ~Impl() {
        if (!joined) return;
        try {
            transaction.lock(true);
            const Unlock release{transaction};
            members.unlock();
            if (members.try_lock(true)) clear();
        } catch (...) {
            // Destructors cannot report I/O failure. The next first member retries
            // cleanup. OS locks are still released by the FileLock destructors.
        }
    }

    std::filesystem::path slot_path(unsigned slot) const {
        return root / ("slot-" + std::to_string(slot) + ".ghclip");
    }

    void clear() const {
        for (unsigned slot = 0; slot < 10; ++slot) std::filesystem::remove(slot_path(slot));
        std::filesystem::remove(root / "default.slot");
        std::filesystem::remove(root / "pending.tmp");
        // Never unlink lock files: another process may already hold the same inode.
    }
};

ClipboardSession::ClipboardSession(std::unique_ptr<Impl> implementation) : impl_(std::move(implementation)) {}
ClipboardSession::~ClipboardSession() = default;

std::expected<std::unique_ptr<ClipboardSession>, std::string>
ClipboardSession::join(const std::filesystem::path& directory) {
    try {
        return std::unique_ptr<ClipboardSession>(new ClipboardSession(std::make_unique<Impl>(directory)));
    } catch (const std::exception& e) { return std::unexpected(e.what()); }
}

std::expected<void, std::string> ClipboardSession::write(unsigned slot, const Stamp& stamp) {
    if (slot > 9) return std::unexpected("Clipboard slot must be 0 through 9");
    const auto bytes = encode_stamp(stamp);
    if (!bytes) return std::unexpected(bytes.error());
    try {
        const std::lock_guard guard(impl_->mutex);
        impl_->transaction.lock(true);
        const Unlock release{impl_->transaction};
        atomic_write(impl_->slot_path(slot), *bytes);
        atomic_write(impl_->root / "default.slot", std::string(1, static_cast<char>('0' + slot)));
        return {};
    } catch (const std::exception& e) { return std::unexpected(e.what()); }
}

std::expected<Stamp, std::string> ClipboardSession::read(unsigned slot) {
    if (slot > 9) return std::unexpected("Clipboard slot must be 0 through 9");
    try {
        const std::lock_guard guard(impl_->mutex);
        impl_->transaction.lock(true);
        const Unlock release{impl_->transaction};
        unsigned resolved = slot;
        if (slot == 0) {
            if (const auto index = read_file(impl_->root / "default.slot", 1)) {
                if (index->size() != 1 || (*index)[0] < '0' || (*index)[0] > '9') {
                    return std::unexpected("Invalid default clipboard index");
                }
                resolved = static_cast<unsigned>((*index)[0] - '0');
            }
        }
        const auto bytes = read_file(impl_->slot_path(resolved), max_clipboard_bytes);
        if (!bytes) return Stamp{};
        auto stamp = decode_stamp(*bytes);
        if (!stamp) return std::unexpected(stamp.error());
        if (slot != 0) atomic_write(impl_->root / "default.slot", std::string(1, static_cast<char>('0' + slot)));
        return stamp;
    } catch (const std::exception& e) { return std::unexpected(e.what()); }
}
} // namespace gatehaven
