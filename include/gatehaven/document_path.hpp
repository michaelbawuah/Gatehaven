#pragma once
#include "gatehaven/document.hpp"
namespace gatehaven {
[[nodiscard]] inline std::filesystem::path document_save_path(std::filesystem::path path, int filter) {
    if (!path.has_extension()) path += filter == 1 ? ".ccsb" : ".ghv";
    return path;
}
}
