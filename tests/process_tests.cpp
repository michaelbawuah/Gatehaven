#include "test.hpp"
#include "gatehaven/resources.hpp"
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

TEST("file URIs preserve Unicode and escape browser control characters") {
    const auto uri = file_uri(utf8_path("manual #100% é.html"));
    CHECK(uri && uri->starts_with("file://"));
    CHECK(uri->find("%23") != std::string::npos && uri->find("%25") != std::string::npos);
    CHECK(uri->find("%C3%A9") != std::string::npos && uri->find(' ') == std::string::npos);
}
