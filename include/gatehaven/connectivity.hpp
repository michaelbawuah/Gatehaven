#pragma once
#include "gatehaven/types.hpp"
namespace gatehaven {
[[nodiscard]] constexpr bool is_relay(Element element) {
    return element == Element::positive_relay || element == Element::negative_relay;
}
[[nodiscard]] constexpr bool receives_signal(Element element) {
    return element >= Element::positive_relay && element <= Element::file_output;
}
[[nodiscard]] constexpr bool conducts_between(Element a, Element b) {
    return a != Element::empty && b != Element::empty &&
        !(a == Element::signal && receives_signal(b)) &&
        !(b == Element::signal && receives_signal(a));
}
}
