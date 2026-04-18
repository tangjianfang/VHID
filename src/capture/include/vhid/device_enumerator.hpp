// Enumerate and monitor HID devices on the system.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace vhid {

/// Summary information about a connected HID device.
struct HidDeviceInfo {
    std::string   device_path;
    std::uint16_t vendor_id      = 0;
    std::uint16_t product_id     = 0;
    std::uint16_t version_number = 0;
    std::string   product;
    std::string   manufacturer;
};

/// Return every HID device currently visible to the system.
std::vector<HidDeviceInfo> enumerate_hid_devices();

/// Poll for a newly-plugged HID device.  Calls |on_new_device| when a device
/// appears that was not present at the start.  Return true from the callback
/// to stop watching.  Returns false on timeout.
bool watch_for_new_device(
    std::function<bool(const HidDeviceInfo&)> on_new_device,
    int timeout_seconds = 60);

} // namespace vhid
