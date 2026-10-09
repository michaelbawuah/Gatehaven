#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace gatehaven::ui {
using InstanceLauncher = std::function<std::expected<void, std::string>(std::optional<std::filesystem::path>)>;
[[nodiscard]] std::expected<void, std::string> launch_instance(std::optional<std::filesystem::path> document);
[[nodiscard]] std::expected<void, std::string> launch_demo(std::string_view name);
// Runs the executable's headless smoke test in a child process and waits for its result.
[[nodiscard]] std::expected<void, std::string> test_child_process();
}
