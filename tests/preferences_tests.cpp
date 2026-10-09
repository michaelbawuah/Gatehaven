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

TEST("contrast preferences round trip while version one keeps the original appearance") {
    Preferences settings; settings.high_contrast = true;
    CHECK(decode_preferences(encode_preferences(settings)).value() == settings);
    const auto legacy = "GATEHAVEN-PREFERENCES 1\n5 0\n0 wire\n1 wire\n2 wire\n3 wire\n4 wire\n0 wire\n";
    CHECK(decode_preferences(legacy).value() == Preferences{});
    CHECK(!decode_preferences("GATEHAVEN-PREFERENCES 2\n5 0 2\n"));
    CHECK(!decode_preferences("GATEHAVEN-PREFERENCES 2\n5 0 -1\n"));
}
