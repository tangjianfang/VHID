// Tests for vhid_capture: DeviceProfile JSON round-trip + descriptor builder.
#include "vhid_test.hpp"

#include "vhid/descriptor_builder.hpp"
#include "vhid/device_profile.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {

vhid::DeviceProfile make_sample_profile() {
    vhid::DeviceProfile p;
    p.vendor_id      = 0x046D;
    p.product_id     = 0xC077;
    p.version_number = 0x0102;
    p.manufacturer   = "Logitech";
    p.product        = "USB Optical Mouse";
    p.serial_number  = "S/N-12345";
    p.usage_page     = 0x01;
    p.usage          = 0x02;

    vhid::ReportInfo input;
    input.report_id   = 0;
    input.byte_length = 4;

    vhid::FieldCaps buttons;
    buttons.usage_page   = 0x09;
    buttons.usage_min    = 1;
    buttons.usage_max    = 3;
    buttons.logical_min  = 0;
    buttons.logical_max  = 1;
    buttons.report_size  = 1;
    buttons.report_count = 3;
    buttons.is_button    = true;
    buttons.is_absolute  = true;
    input.fields.push_back(buttons);

    vhid::FieldCaps padding;
    padding.usage_page   = 0x09;
    padding.report_size  = 1;
    padding.report_count = 5;
    input.fields.push_back(padding);

    vhid::FieldCaps xy;
    xy.usage_page   = 0x01;
    xy.usage_min    = 0x30;   // X
    xy.usage_max    = 0x31;   // Y
    xy.logical_min  = -127;
    xy.logical_max  = 127;
    xy.report_size  = 8;
    xy.report_count = 2;
    xy.is_button    = false;
    xy.is_absolute  = false;
    input.fields.push_back(xy);

    p.input_reports.push_back(input);

    vhid::ReportInfo feat;
    feat.report_id   = 5;
    feat.byte_length = 9;
    vhid::FieldCaps fopt;
    fopt.usage_page   = 0xFF00;
    fopt.usage_min    = 0xC0;
    fopt.usage_max    = 0xC0;
    fopt.logical_min  = 0;
    fopt.logical_max  = 255;
    fopt.report_size  = 8;
    fopt.report_count = 8;
    feat.fields.push_back(fopt);
    p.feature_reports.push_back(feat);

    p.report_descriptor = {0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0xC0};
    return p;
}

} // namespace

TEST_CASE("DeviceProfile JSON round-trip") {
    auto path = (std::filesystem::temp_directory_path() / "vhid_profile_test.json").string();
    auto orig = make_sample_profile();
    REQUIRE(orig.save(path));

    auto loaded_opt = vhid::DeviceProfile::load(path);
    REQUIRE(loaded_opt.has_value());
    const auto& loaded = *loaded_opt;

    CHECK(loaded.vendor_id      == orig.vendor_id);
    CHECK(loaded.product_id     == orig.product_id);
    CHECK(loaded.version_number == orig.version_number);
    CHECK(loaded.manufacturer   == orig.manufacturer);
    CHECK(loaded.product        == orig.product);
    CHECK(loaded.serial_number  == orig.serial_number);
    CHECK(loaded.usage_page     == orig.usage_page);
    CHECK(loaded.usage          == orig.usage);
    REQUIRE(loaded.input_reports.size()   == orig.input_reports.size());
    REQUIRE(loaded.feature_reports.size() == orig.feature_reports.size());
    REQUIRE(loaded.input_reports[0].fields.size() == orig.input_reports[0].fields.size());

    const auto& xy_orig   = orig.input_reports[0].fields[2];
    const auto& xy_loaded = loaded.input_reports[0].fields[2];
    CHECK(xy_loaded.usage_min   == xy_orig.usage_min);
    CHECK(xy_loaded.usage_max   == xy_orig.usage_max);
    CHECK(xy_loaded.logical_min == xy_orig.logical_min);
    CHECK(xy_loaded.logical_max == xy_orig.logical_max);
    CHECK(xy_loaded.is_absolute == xy_orig.is_absolute);

    CHECK(loaded.report_descriptor == orig.report_descriptor);

    std::remove(path.c_str());
}

TEST_CASE("DeviceProfile::load returns nullopt for missing file") {
    auto missing = (std::filesystem::temp_directory_path() / "vhid_no_such.json").string();
    std::remove(missing.c_str());
    auto loaded = vhid::DeviceProfile::load(missing);
    CHECK(!loaded.has_value());
}

TEST_CASE("build_report_descriptor emits expected key bytes") {
    auto p = make_sample_profile();
    p.report_descriptor.clear();
    auto d = vhid::build_report_descriptor(p);
    REQUIRE(d.size() >= 8);

    // First items must be: Usage Page (Generic Desktop = 0x01), Usage (Mouse = 0x02),
    // Collection (Application = 0x01).
    CHECK(d[0] == 0x05);                   // Usage Page (1-byte)
    CHECK(d[1] == 0x01);                   // value
    CHECK(d[2] == 0x09);                   // Usage (1-byte)
    CHECK(d[3] == 0x02);                   // value
    CHECK(d[4] == 0xA1);                   // Collection (1-byte)
    CHECK(d[5] == 0x01);                   // Application

    // Last byte must be End Collection (0xC0).
    CHECK(d.back() == 0xC0);

    // Must contain a Logical Maximum item with value 127 somewhere (X/Y range).
    bool has_lmax_127 = false;
    for (std::size_t i = 0; i + 1 < d.size(); ++i) {
        if (d[i] == 0x25 && d[i + 1] == 0x7F) { has_lmax_127 = true; break; }
    }
    CHECK(has_lmax_127);
}
