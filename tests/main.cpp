#include "test.hpp"

#include <exception>
#include <iostream>

int main() {
    std::size_t failed = 0;
    for (const auto& item : test::cases()) {
        try {
            item.run();
            std::cout << "PASS " << item.name << '\n';
        } catch (const std::exception& e) {
            ++failed;
            std::cerr << "FAIL " << item.name << ": " << e.what() << '\n';
        }
    }
    std::cout << test::cases().size() - failed << '/' << test::cases().size() << " tests passed\n";
    return failed == 0 ? 0 : 1;
}
