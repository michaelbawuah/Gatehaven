#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace gatehaven {
// Resolve the running executable independently of argv[0] and the working directory.
[[nodiscard]] std::expected<std::filesystem::path, std::string> current_executable();
}
