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

TEST("a correct checksum does not bypass clipboard structure validation") {
    const auto valid = encode_stamp({4, 2, {{0, 0, Element::wire}, {3, 1, Element::source}}}).value();
    const auto reseal = [](std::string bytes) {
        std::uint64_t hash = 14695981039346656037ULL;
        for (std::size_t i = 0; i < bytes.size() - 8; ++i) {
            hash = (hash ^ static_cast<unsigned char>(bytes[i])) * 1099511628211ULL;
        }
        for (unsigned i = 0; i < 8; ++i) bytes[bytes.size() - 8 + i] = static_cast<char>((hash >> (8 * i)) & 255);
        return bytes;
    };
    auto invalid = valid;
    invalid[32] = 4; // x == width
    CHECK(!decode_stamp(reseal(invalid)));
    invalid = valid; invalid[40] = 0; // Empty element
    CHECK(!decode_stamp(reseal(invalid)));
    invalid = valid; invalid[41] = 0; invalid[45] = 0; // Duplicate first cell
    CHECK(!decode_stamp(reseal(invalid)));
    invalid = valid; invalid[12] = 2; // Width exceeds 2^32
    CHECK(!decode_stamp(reseal(invalid)));
    invalid = valid; invalid[24] = static_cast<char>(255); // Forged count
    CHECK(!decode_stamp(reseal(invalid)));
}
