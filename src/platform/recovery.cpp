#include "gatehaven/recovery.hpp"
#include "session_lock.hpp"
#include <random>
#include <algorithm>
#include <sstream>

namespace gatehaven {
namespace {
bool valid_id(std::string_view id) {
    return id.starts_with("session-") && id.size() <= 80 && id.size() > 8 &&
        std::all_of(id.begin() + 8, id.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || c == '-'; });
}
std::string make_id() {
    std::random_device random;
    std::ostringstream text;
    text << "session-" << std::hex << static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())
         << '-' << random() << '-' << random();
    return text.str();
}
}
struct RecoveryStore::Impl {
    std::filesystem::path root;
    std::string id;
    platform::FileLock owner;
    explicit Impl(const std::filesystem::path& directory)
        : root(platform::prepare_private_directory(directory)), id(make_id()), owner(root / (id + ".lock")) {
        if (!owner.try_lock(true) || std::filesystem::exists(root / (id + ".ghv"))) throw std::runtime_error("Recovery identity collision");
    }
    std::filesystem::path snapshot() const { return root / (id + ".ghv"); }
};
RecoveryStore::RecoveryStore(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
RecoveryStore::~RecoveryStore() = default;
std::expected<void, std::string> RecoveryStore::write(const Circuit& circuit) {
    if (circuit.size() > DocumentLimits{}.max_cells) return std::unexpected("Recovery exceeds document cell limit");
    const auto result = save_document(impl_->snapshot(), circuit);
    if (!result) return std::unexpected(result.error().message);
    return {};
}
std::expected<void, std::string> RecoveryStore::discard() {
    std::error_code error; std::filesystem::remove(impl_->snapshot(), error);
    if (error) return std::unexpected(error.message());
    return {};
}
std::expected<std::vector<RecoveryEntry>, std::string> RecoveryStore::scan() const {
    try {
        std::vector<RecoveryEntry> entries;
        std::size_t inspected = 0;
        for (const auto& entry : std::filesystem::directory_iterator(impl_->root)) {
            if (++inspected > 10000) return std::unexpected("Recovery directory contains too many entries");
            if (entry.path().extension() != ".ghv" || !std::filesystem::is_regular_file(entry.symlink_status())) continue;
            const auto candidate = entry.path().stem().string();
            if (!valid_id(candidate) || candidate == impl_->id) continue;
            platform::FileLock owner(impl_->root / (candidate + ".lock"));
            if (!owner.try_lock(true)) continue;
            std::error_code error;
            const auto size = std::filesystem::file_size(entry.path(), error);
            if (error == std::errc::no_such_file_or_directory) continue;
            if (error) return std::unexpected(error.message());
            const auto modified = std::filesystem::last_write_time(entry.path(), error);
            if (error == std::errc::no_such_file_or_directory) continue;
            if (error) return std::unexpected(error.message());
            entries.push_back({candidate, modified, size});
        }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.modified > b.modified; });
        return entries;
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
std::expected<Circuit, std::string> RecoveryStore::restore(std::string_view candidate) {
    if (!valid_id(candidate) || candidate == impl_->id) return std::unexpected("Invalid recovery identity");
    try {
        const auto path = impl_->root / (std::string(candidate) + ".ghv");
        platform::FileLock owner(impl_->root / (std::string(candidate) + ".lock"));
        if (!owner.try_lock(true)) return std::unexpected("This circuit is open in another window");
        if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(path))) return std::unexpected("Recovery snapshot is not a regular file");
        auto circuit = load_document(path);
        if (!circuit) return std::unexpected(circuit.error().message);
        const auto saved = write(*circuit); // Preserve ownership before consuming the abandoned copy.
        if (!saved) return std::unexpected(saved.error());
        std::error_code error; std::filesystem::remove(path, error);
        return std::move(*circuit);
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
std::expected<void, std::string> RecoveryStore::remove(std::string_view candidate) {
    if (!valid_id(candidate) || candidate == impl_->id) return std::unexpected("Invalid recovery identity");
    try {
        platform::FileLock owner(impl_->root / (std::string(candidate) + ".lock"));
        if (!owner.try_lock(true)) return std::unexpected("This circuit is open in another window");
        std::filesystem::remove(impl_->root / (std::string(candidate) + ".ghv"));
        return {};
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
const std::string& RecoveryStore::id() const { return impl_->id; }
std::expected<std::unique_ptr<RecoveryStore>, std::string> RecoveryStore::open(const std::filesystem::path& directory) {
    try { return std::unique_ptr<RecoveryStore>(new RecoveryStore(std::make_unique<Impl>(directory))); }
    catch (const std::exception& error) { return std::unexpected(error.what()); }
}
}
