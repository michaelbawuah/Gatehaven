#include "test.hpp"
#include "gatehaven/process.hpp"

TEST("the running executable is resolved independently of the working directory") {
    const auto before = gatehaven::current_executable();
    CHECK(before && before->is_absolute() && std::filesystem::is_regular_file(*before));
    const auto original = std::filesystem::current_path();
    struct Restore {
        std::filesystem::path path;
        ~Restore() { std::error_code error; std::filesystem::current_path(path, error); }
    } restore{original};
    std::filesystem::current_path(std::filesystem::temp_directory_path());
    const auto after = gatehaven::current_executable();
    CHECK(after && *before == *after);
}
