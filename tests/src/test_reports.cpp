#include "vhid_test.hpp"
#include "vhid/reports.hpp"

#include <cstdint>
#include <stdexcept>

using namespace vhid;

TEST_CASE("InputReport carries report id and zero-padded payload") {
    InputReport r;
    REQUIRE(r.wire.size() == InputReport::kWireSize);
    REQUIRE(r.wire[0] == kInputReportId);
    for (std::size_t i = 1; i < r.wire.size(); ++i) CHECK(r.wire[i] == 0);

    std::uint8_t payload[3] = {0xAA, 0xBB, 0xCC};
    r.set_payload(payload, sizeof(payload));
    CHECK(r.payload()[0] == 0xAA);
    CHECK(r.payload()[1] == 0xBB);
    CHECK(r.payload()[2] == 0xCC);
    CHECK(r.payload()[3] == 0x00);   // padding
}

TEST_CASE("set_payload rejects oversize input") {
    OutputReport r;
    std::uint8_t big[OutputReport::kPayloadSize + 1] = {};
    bool threw = false;
    try { r.set_payload(big, sizeof(big)); }
    catch (const std::length_error&) { threw = true; }
    CHECK(threw);
}

TEST_CASE("parse_input_report round-trips") {
    InputReport r;
    std::uint8_t payload[InputReport::kPayloadSize];
    for (std::size_t i = 0; i < sizeof(payload); ++i) payload[i] = static_cast<std::uint8_t>(i);
    r.set_payload(payload, sizeof(payload));

    auto parsed = parse_input_report(r.wire.data(), r.wire.size());
    CHECK(parsed == r);
}

TEST_CASE("parse rejects wrong report id") {
    InputReport r;
    bool threw = false;
    try { (void)parse_output_report(r.wire.data(), r.wire.size()); }
    catch (const std::length_error&) { threw = true; }
    catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}
