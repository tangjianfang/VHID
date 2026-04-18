#include "vhid/descriptor_builder.hpp"

#include <cstdint>
#include <vector>

namespace vhid {

namespace {

// HID short-item tags (bits 4-7), types (bits 2-3), sizes (bits 0-1).
// See HID 1.11 sec. 6.2.2.

enum ItemType : std::uint8_t {
    TYPE_MAIN   = 0x0,
    TYPE_GLOBAL = 0x1,
    TYPE_LOCAL  = 0x2,
};

// Main item tags
constexpr std::uint8_t TAG_INPUT          = 0x8;
constexpr std::uint8_t TAG_OUTPUT         = 0x9;
constexpr std::uint8_t TAG_FEATURE        = 0xB;
constexpr std::uint8_t TAG_COLLECTION     = 0xA;
constexpr std::uint8_t TAG_END_COLLECTION = 0xC;

// Global item tags
constexpr std::uint8_t TAG_USAGE_PAGE   = 0x0;
constexpr std::uint8_t TAG_LOGICAL_MIN  = 0x1;
constexpr std::uint8_t TAG_LOGICAL_MAX  = 0x2;
constexpr std::uint8_t TAG_REPORT_SIZE  = 0x7;
constexpr std::uint8_t TAG_REPORT_ID    = 0x8;
constexpr std::uint8_t TAG_REPORT_COUNT = 0x9;

// Local item tags
constexpr std::uint8_t TAG_USAGE     = 0x0;
constexpr std::uint8_t TAG_USAGE_MIN = 0x1;
constexpr std::uint8_t TAG_USAGE_MAX = 0x2;

// Main-item data flag bits (Input/Output/Feature)
constexpr std::uint8_t MAIN_DATA      = 0x00; // (Data, not Constant)
constexpr std::uint8_t MAIN_CONSTANT  = 0x01;
constexpr std::uint8_t MAIN_VARIABLE  = 0x02; // (Variable, not Array)
constexpr std::uint8_t MAIN_ARRAY     = 0x00;
constexpr std::uint8_t MAIN_ABSOLUTE  = 0x00;
constexpr std::uint8_t MAIN_RELATIVE  = 0x04;
constexpr std::uint8_t MAIN_NULLSTATE = 0x40;

void emit(std::vector<std::uint8_t>& out,
          std::uint8_t tag, std::uint8_t type, std::int64_t data, bool force_size = false) {
    // Pick the smallest size that fits the data (signed for min/max can be negative,
    // but HID treats this field as raw bytes — caller passes already in two's complement range).
    std::uint8_t size_bits = 0;   // encodes 0/1/2/4 bytes as 0/1/2/3
    std::size_t  data_bytes = 0;

    auto fits_in = [](std::int64_t v, int bytes) {
        if (bytes == 1) return v >= -128 && v <= 255;
        if (bytes == 2) return v >= -32768 && v <= 65535;
        return true;
    };

    if (data == 0 && !force_size) {
        size_bits = 0;
        data_bytes = 0;
    } else if (fits_in(data, 1)) {
        size_bits = 1;
        data_bytes = 1;
    } else if (fits_in(data, 2)) {
        size_bits = 2;
        data_bytes = 2;
    } else {
        size_bits = 3;
        data_bytes = 4;
    }

    std::uint8_t prefix = static_cast<std::uint8_t>((tag << 4) | (type << 2) | size_bits);
    out.push_back(prefix);
    auto u = static_cast<std::uint64_t>(data);
    for (std::size_t i = 0; i < data_bytes; ++i) {
        out.push_back(static_cast<std::uint8_t>((u >> (8 * i)) & 0xFF));
    }
}

void emit_usage_page(std::vector<std::uint8_t>& out, std::uint16_t page) {
    emit(out, TAG_USAGE_PAGE, TYPE_GLOBAL, page, true);
}

void emit_report_section(std::vector<std::uint8_t>& out,
                         const ReportInfo& r, std::uint8_t main_tag) {
    if (r.fields.empty()) return;
    if (r.report_id != 0) {
        emit(out, TAG_REPORT_ID, TYPE_GLOBAL, r.report_id, true);
    }
    for (const auto& f : r.fields) {
        emit_usage_page(out, f.usage_page);
        if (f.usage_min == f.usage_max) {
            emit(out, TAG_USAGE, TYPE_LOCAL, f.usage_min, true);
        } else {
            emit(out, TAG_USAGE_MIN, TYPE_LOCAL, f.usage_min, true);
            emit(out, TAG_USAGE_MAX, TYPE_LOCAL, f.usage_max, true);
        }
        emit(out, TAG_LOGICAL_MIN,  TYPE_GLOBAL, f.logical_min,  true);
        emit(out, TAG_LOGICAL_MAX,  TYPE_GLOBAL, f.logical_max,  true);
        emit(out, TAG_REPORT_SIZE,  TYPE_GLOBAL, f.report_size,  true);
        emit(out, TAG_REPORT_COUNT, TYPE_GLOBAL, f.report_count, true);

        std::uint8_t flags = MAIN_DATA;
        flags |= f.is_button ? MAIN_VARIABLE : (f.is_absolute ? MAIN_VARIABLE : MAIN_VARIABLE);
        // For value items: Variable + Absolute/Relative.  For buttons: Variable + Absolute.
        if (!f.is_absolute) flags |= MAIN_RELATIVE;
        if (f.has_null)     flags |= MAIN_NULLSTATE;
        emit(out, main_tag, TYPE_MAIN, flags, true);
    }
}

} // namespace

std::vector<std::uint8_t> build_report_descriptor(const DeviceProfile& profile) {
    std::vector<std::uint8_t> out;
    out.reserve(64);

    emit_usage_page(out, profile.usage_page ? profile.usage_page : 0xFF00);
    emit(out, TAG_USAGE, TYPE_LOCAL, profile.usage ? profile.usage : 0x01, true);
    emit(out, TAG_COLLECTION, TYPE_MAIN, 0x01, true); // Application

    for (const auto& r : profile.input_reports)   emit_report_section(out, r, TAG_INPUT);
    for (const auto& r : profile.output_reports)  emit_report_section(out, r, TAG_OUTPUT);
    for (const auto& r : profile.feature_reports) emit_report_section(out, r, TAG_FEATURE);

    emit(out, TAG_END_COLLECTION, TYPE_MAIN, 0, false);
    return out;
}

} // namespace vhid
