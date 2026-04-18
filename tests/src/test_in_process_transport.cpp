#include "vhid_test.hpp"
#include "vhid/in_process_transport.hpp"

using namespace vhid;

TEST_CASE("in-process: input report flows device -> host") {
    InProcessMockTransport t;
    t.open();

    InputReport seen;
    bool got = false;
    t.on_input([&](const InputReport& r) { seen = r; got = true; });

    InputReport sent;
    std::uint8_t payload[] = {1, 2, 3, 4};
    sent.set_payload(payload, sizeof(payload));
    t.submit_input(sent);

    REQUIRE(got);
    CHECK(seen == sent);
    t.close();
}

TEST_CASE("in-process: output report flows host -> device") {
    InProcessMockTransport t;
    t.open();

    OutputReport seen;
    bool got = false;
    t.on_output([&](const OutputReport& r) { seen = r; got = true; });

    OutputReport sent;
    std::uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    sent.set_payload(payload, sizeof(payload));
    t.send_output(sent);

    REQUIRE(got);
    CHECK(seen == sent);
}

TEST_CASE("in-process: feature get/set round-trip") {
    InProcessMockTransport t;
    t.open();

    FeatureReport stored;
    t.on_feature_set([&](const FeatureReport& r) { stored = r; });
    t.on_feature_get([&] { return stored; });

    FeatureReport sent;
    std::uint8_t payload[] = {7, 7, 7};
    sent.set_payload(payload, sizeof(payload));
    t.set_feature(sent);
    auto got = t.get_feature();

    CHECK(got == sent);
}

TEST_CASE("in-process: closed transport drops events") {
    InProcessMockTransport t;
    int hits = 0;
    t.on_input([&](const InputReport&) { ++hits; });
    // No open() call.
    InputReport r;
    t.submit_input(r);
    CHECK(hits == 0);
}
