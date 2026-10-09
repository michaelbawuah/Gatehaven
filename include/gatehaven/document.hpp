#pragma once

#include "gatehaven/circuit.hpp"

#include <expected>
#include <filesystem>
#include <iosfwd>
#include <string>

namespace gatehaven {

enum class DocumentFormat { automatic, native, legacy };
[[nodiscard]] DocumentFormat document_format(const std::filesystem::path& path);

struct DocumentError {
    std::size_t line{};
    std::string message;
};

struct DocumentLimits {
    std::size_t max_cells{1'000'000};
    std::size_t max_lines{2'000'000};
    std::size_t max_line_bytes{192};
    std::uint64_t max_legacy_area{64'000'000};
};

[[nodiscard]] std::expected<Circuit, DocumentError> read_document(std::istream& input,
                                                               DocumentLimits limits = {});
[[nodiscard]] std::expected<void, DocumentError> write_document(std::ostream& output,
                                                              const Circuit& circuit);
[[nodiscard]] std::expected<Circuit, DocumentError> load_document(const std::filesystem::path& path,
                                                               DocumentLimits limits = {});
[[nodiscard]] std::expected<void, DocumentError> save_document(const std::filesystem::path& path,
                                                             const Circuit& circuit,
                                                             DocumentFormat format = DocumentFormat::automatic,
                                                             DocumentLimits limits = {});

} // namespace gatehaven
