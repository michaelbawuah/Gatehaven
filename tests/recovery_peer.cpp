#include "gatehaven/recovery.hpp"
#include <iostream>
#include <string>
int main(int argc, char** argv) {
    using namespace gatehaven;
    if (argc < 3) return 2;
    auto opened = RecoveryStore::open(argv[1]);
    if (!opened) { std::cerr << opened.error(); return 1; }
    auto& store = **opened;
    const std::string mode = argv[2];
    if (mode == "hold") {
        Circuit circuit; circuit.set({-7, 11}, Element::source);
        if (!store.write(circuit)) return 1;
        std::cout << store.id() << std::endl;
        std::string line; std::getline(std::cin, line); return 0;
    }
    if (mode == "scan") {
        const auto entries = store.scan(); if (!entries) return 1;
        for (const auto& entry : *entries) std::cout << entry.id << '\n';
        return 0;
    }
    if (mode == "restore" && argc == 4) {
        const auto restored = store.restore(argv[3]);
        if (!restored) { std::cerr << restored.error(); return 1; }
        if (restored->size() != 1 || restored->at({-7, 11}) != Element::source) return 1;
        return store.discard() ? 0 : 1;
    }
    return 2;
}
