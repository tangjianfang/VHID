// Inspect a HID device and capture a full DeviceProfile.
#pragma once

#include "vhid/device_profile.hpp"

#include <optional>
#include <string>

namespace vhid {

/// Open a HID device by its device path and capture a full DeviceProfile
/// including attributes, strings, capabilities, and a reconstructed
/// report descriptor.
std::optional<DeviceProfile> inspect_device(const std::string& device_path);

} // namespace vhid
