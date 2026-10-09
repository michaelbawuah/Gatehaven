#pragma once
#include "gatehaven/simulation.hpp"
#include <charconv>

namespace gatehaven {
// Versioned FNV-1a reproducibility fingerprint; not a cryptographic checksum.
[[nodiscard]] inline std::string state_digest(const Circuit& circuit, const Simulation& simulation) {
    std::uint64_t hash = 14695981039346656037ULL;
    const auto integer = [&](std::uint64_t value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i) { hash ^= (value >> (i * 8)) & 255; hash *= 1099511628211ULL; }
    };
    integer(1, 1); integer(simulation.ticks(), 8); integer(circuit.size(), 8);
    if (const auto bounds = circuit.bounds()) circuit.visit(*bounds, [&](Cell cell) {
        integer(static_cast<std::uint32_t>(cell.position.x), 4);
        integer(static_cast<std::uint32_t>(cell.position.y), 4);
        integer(static_cast<std::uint8_t>(cell.element), 1);
        integer(simulation.ports(cell.position), 1);
        integer(simulation.sent(cell.position), 1);
        integer(simulation.received(cell.position), 1);
    });
    std::array<char, 16> buffer{};
    const auto end = std::to_chars(buffer.data(), buffer.data() + buffer.size(), hash, 16).ptr;
    return std::string(16 - static_cast<std::size_t>(end - buffer.data()), '0') + std::string(buffer.data(), end);
}
}
