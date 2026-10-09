#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace test {
struct Case { const char* name; void (*run)(); };
inline std::vector<Case>& cases() {
    static std::vector<Case> collection;
    return collection;
}
struct Register {
    Register(const char* name, void (*run)()) { cases().push_back({name, run}); }
};
inline void check(bool ok, const char* expression, const char* file, int line) {
    if (!ok) throw std::runtime_error(std::string(file) + ":" + std::to_string(line) +
                                     ": " + expression);
}
} // namespace test

#define GH_JOIN_IMPL(a, b) a##b
#define GH_JOIN(a, b) GH_JOIN_IMPL(a, b)
#define TEST(name) \
    static void GH_JOIN(test_, __LINE__)(); \
    static const test::Register GH_JOIN(reg_, __LINE__)(name, GH_JOIN(test_, __LINE__)); \
    static void GH_JOIN(test_, __LINE__)()
#define CHECK(...) test::check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__, __FILE__, __LINE__)
