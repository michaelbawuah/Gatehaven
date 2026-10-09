#pragma once

#include "gatehaven/editor.hpp"

#include <filesystem>
#include <memory>

namespace gatehaven {

// All processes using the same private directory belong to one session. OS
// locks track live members; clipboard data is cleared at session boundaries.
class ClipboardSession {
public:
    [[nodiscard]] static std::expected<std::unique_ptr<ClipboardSession>, std::string>
        join(const std::filesystem::path& directory);
    ~ClipboardSession();
    ClipboardSession(const ClipboardSession&) = delete;
    ClipboardSession& operator=(const ClipboardSession&) = delete;

    // Slot zero follows the most recently copied/pasted explicit slot. Copying
    // directly to zero creates its own value until another explicit slot is used.
    [[nodiscard]] std::expected<Stamp, std::string> read(unsigned slot);
    [[nodiscard]] std::expected<void, std::string> write(unsigned slot, const Stamp& stamp);

private:
    struct Impl;
    explicit ClipboardSession(std::unique_ptr<Impl> implementation);
    std::unique_ptr<Impl> impl_;
};

} // namespace gatehaven
