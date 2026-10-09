#include "instances.hpp"
#include "gatehaven/process.hpp"

#include <SDL3/SDL.h>

#include <memory>

namespace gatehaven::ui {
namespace {
std::expected<void, std::string> launch(std::string argument, bool background) {
    const auto executable = current_executable();
    if (!executable) return std::unexpected(executable.error());
    const auto utf8 = executable->u8string();
    const std::string program(utf8.begin(), utf8.end());
    const char* arguments[]{program.c_str(), argument.c_str(), nullptr};
    const auto properties = SDL_CreateProperties();
    if (!properties) return std::unexpected(SDL_GetError());
    struct Properties {
        SDL_PropertiesID id;
        ~Properties() { SDL_DestroyProperties(id); }
    } owner{properties};
    if (!SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, arguments) ||
        !SDL_SetBooleanProperty(properties, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, background)) {
        return std::unexpected(SDL_GetError());
    }
    const std::unique_ptr<SDL_Process, decltype(&SDL_DestroyProcess)> process(
        SDL_CreateProcessWithProperties(properties), SDL_DestroyProcess);
    if (!process) return std::unexpected(SDL_GetError());
    if (!background) {
        int exit_code{};
        if (!SDL_WaitProcess(process.get(), true, &exit_code)) return std::unexpected(SDL_GetError());
        if (exit_code != 0) return std::unexpected("Gatehaven child test failed: " + std::to_string(exit_code));
    }
    return {};
}
}

std::expected<void, std::string> launch_instance(std::optional<std::filesystem::path> document) {
    if (!document) return launch("--new", true);
    const auto utf8 = std::filesystem::absolute(*document).u8string();
    return launch(std::string(utf8.begin(), utf8.end()), true);
}

std::expected<void, std::string> launch_demo(std::string_view name) { return launch("--demo=" + std::string(name), true); }

std::expected<void, std::string> test_child_process() { return launch("--self-test-child", false); }
}
