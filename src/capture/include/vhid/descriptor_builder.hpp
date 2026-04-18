// Reconstruct a HID Report Descriptor from captured capabilities.
#pragma once

#include "vhid/device_profile.hpp"

#include <cstdint>
#include <vector>

namespace vhid {

/// Build a standards-compliant HID Report Descriptor from the button/value
/// capabilities stored in a DeviceProfile.  The result is byte-for-byte
/// suitable for VHF_CONFIG or any HID minidriver.
std::vector<std::uint8_t> build_report_descriptor(const DeviceProfile& profile);

} // namespace vhid
