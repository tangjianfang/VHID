#include "vhid_test.hpp"
#include "vhid/report_descriptor.hpp"

using namespace vhid;

TEST_CASE("descriptor is non-empty and starts with vendor usage page") {
    REQUIRE(kReportDescriptorSize > 0);
    // Usage Page (Vendor-Defined): 06 00 FF
    CHECK(kReportDescriptor[0] == 0x06);
    CHECK(kReportDescriptor[1] == 0x00);
    CHECK(kReportDescriptor[2] == 0xFF);
}

TEST_CASE("descriptor declares all three report ids") {
    bool seen_in = false, seen_out = false, seen_feat = false;
    // Walk forward looking for the Report ID short-item tag (0x85, <id>).
    for (std::size_t i = 0; i + 1 < kReportDescriptorSize; ++i) {
        if (kReportDescriptor[i] == 0x85) {
            std::uint8_t id = kReportDescriptor[i + 1];
            if (id == kInputReportId)   seen_in   = true;
            if (id == kOutputReportId)  seen_out  = true;
            if (id == kFeatureReportId) seen_feat = true;
        }
    }
    CHECK(seen_in);
    CHECK(seen_out);
    CHECK(seen_feat);
}
