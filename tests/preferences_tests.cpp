#include "test.hpp"
#include "gatehaven/preferences.hpp"
using namespace gatehaven;
TEST("preferences preserve independent bindings speed and beginner mode") {
    Preferences p; p.speed = 137; p.beginner = true;
    p.bindings[1] = {ToolKind::pencil, Element::nor_gate};
    CHECK(decode_preferences(encode_preferences(p)).value() == p);
    CHECK(!decode_preferences("GATEHAVEN-PREFERENCES 2\n"));
    CHECK(!decode_preferences(encode_preferences(p) + "unexpected"));
    CHECK(!decode_preferences(std::string(4097, 'x')));
    p.speed = 0; CHECK(!decode_preferences(encode_preferences(p)));
}
