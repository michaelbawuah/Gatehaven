#pragma once
#include "gatehaven/document.hpp"
#include <memory>

namespace gatehaven {
struct RecoveryEntry {
    std::string id;
    std::filesystem::file_time_type modified;
    std::uintmax_t bytes{};
};
// Each live window owns a locked snapshot. Destruction releases ownership but
// preserves data; an orderly exit explicitly discards its snapshot.
class RecoveryStore {
public:
    static std::expected<std::unique_ptr<RecoveryStore>, std::string> open(const std::filesystem::path& directory);
    ~RecoveryStore();
    RecoveryStore(const RecoveryStore&) = delete;
    RecoveryStore& operator=(const RecoveryStore&) = delete;
    [[nodiscard]] const std::string& id() const;
    [[nodiscard]] std::expected<void, std::string> write(const Circuit& circuit);
    [[nodiscard]] std::expected<void, std::string> discard();
private:
    struct Impl;
    explicit RecoveryStore(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};
}
