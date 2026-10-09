#include "test.hpp"
#include "gatehaven/clipboard_codec.hpp"

using namespace gatehaven;

TEST("clipboard encoding preserves empty borders and the full coordinate range") {
    for (const auto& stamp : {Stamp{}, Stamp{9, 4, {}},
             Stamp{4294967296LL, 4294967296LL, {{0, 0, Element::source},
                 {4294967295LL, 4294967295LL, Element::negative_relay}}}}) {
        const auto bytes = encode_stamp(stamp);
        CHECK(bytes);
        CHECK(decode_stamp(*bytes).value() == stamp);
    }
}

TEST("clipboard rejects truncation corruption and trailing data") {
    const auto bytes = encode_stamp({8, 3, {{0, 1, Element::wire}, {7, 2, Element::and_gate}}}).value();
    for (std::size_t length = 0; length < bytes.size(); ++length) {
        CHECK(!decode_stamp(std::string_view(bytes).substr(0, length)));
    }
    for (std::size_t position = 0; position < bytes.size(); ++position) {
        auto damaged = bytes;
        damaged[position] ^= 1;
        CHECK(!decode_stamp(damaged));
    }
    CHECK(!decode_stamp(bytes + 'x'));
    CHECK(!decode_stamp(std::string(max_clipboard_bytes + 1, 'x')));
}

TEST("clipboard validates untrusted dimensions cell types offsets and duplicates") {
    CHECK(!encode_stamp({-1, 1, {}}));
    CHECK(!encode_stamp({1, 0, {}}));
    CHECK(!encode_stamp({4294967297LL, 1, {}}));
    CHECK(!encode_stamp({1, 1, {{-1, 0, Element::wire}}}));
    CHECK(!encode_stamp({1, 1, {{1, 0, Element::wire}}}));
    CHECK(!encode_stamp({1, 1, {{0, 0, Element::empty}}}));
    CHECK(!encode_stamp({1, 1, {{0, 0, static_cast<Element>(255)}}}));
    CHECK(!encode_stamp({1, 1, {{0, 0, Element::wire}, {0, 0, Element::source}}}));
}
