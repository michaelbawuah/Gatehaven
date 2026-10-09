#include "gatehaven/recovery.hpp"
#include "session_lock.hpp"
#include <random>
#include <sstream>

namespace gatehaven {
namespace {
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
const std::string& RecoveryStore::id() const { return impl_->id; }
std::expected<std::unique_ptr<RecoveryStore>, std::string> RecoveryStore::open(const std::filesystem::path& directory) {
    try { return std::unique_ptr<RecoveryStore>(new RecoveryStore(std::make_unique<Impl>(directory))); }
    catch (const std::exception& error) { return std::unexpected(error.what()); }
}
}
