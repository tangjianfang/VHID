// Complete profile of a physical HID device — enough to clone it as a virtual device.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vhid {

/// Describes a single field within an HID report (button or value).
struct FieldCaps {
    std::uint16_t usage_page    = 0;
    std::uint16_t usage_min     = 0;
    std::uint16_t usage_max     = 0;
    std::int32_t  logical_min   = 0;
    std::int32_t  logical_max   = 0;
    std::int32_t  physical_min  = 0;
    std::int32_t  physical_max  = 0;
    std::uint16_t report_size   = 0;   // bits per datum
    std::uint16_t report_count  = 0;   // number of data items
    std::uint32_t unit          = 0;
    std::uint32_t unit_exponent = 0;
    bool          is_button     = false;
    bool          is_absolute   = true;
    bool          has_null      = false;
};

/// Describes one HID report (input, output, or feature) and its fields.
struct ReportInfo {
    std::uint8_t  report_id   = 0;
    std::uint16_t byte_length = 0;   // max wire size (report ID byte + payload)
    std::vector<FieldCaps> fields;
};

/// Complete snapshot of a physical HID device's identity, capabilities,
/// and reconstructed report descriptor.  Serialisable to / from JSON.
struct DeviceProfile {
    // Device identity
    std::uint16_t vendor_id       = 0;
    std::uint16_t product_id      = 0;
    std::uint16_t version_number  = 0;
    std::string   manufacturer;
    std::string   product;
    std::string   serial_number;

    // Top-level HID collection
    std::uint16_t usage_page = 0;
    std::uint16_t usage      = 0;

    // Reports (grouped by report ID)
    std::vector<ReportInfo> input_reports;
    std::vector<ReportInfo> output_reports;
    std::vector<ReportInfo> feature_reports;

    // Reconstructed raw HID report descriptor
    std::vector<std::uint8_t> report_descriptor;

    /// Save this profile to a JSON file.  Returns true on success.
    bool save(const std::string& path) const;

    /// Load a profile from a JSON file.
    static std::optional<DeviceProfile> load(const std::string& path);
};

} // namespace vhid
