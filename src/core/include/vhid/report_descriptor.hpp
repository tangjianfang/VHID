// Shared report descriptor + IDs for the virtual HID device.
// Used by both the user-mode mock transport and the KMDF/VHF driver.
#pragma once

#include <cstddef>
#include <cstdint>

namespace vhid {

// Vendor-defined Usage Page (per HID spec, FF00-FFFF is vendor-defined).
inline constexpr std::uint16_t kVendorUsagePage = 0xFF00;
inline constexpr std::uint16_t kVendorUsage     = 0x0001;

// Default VID/PID for the virtual device. Override in the driver INF if needed.
inline constexpr std::uint16_t kDefaultVendorId  = 0xFEED;
inline constexpr std::uint16_t kDefaultProductId = 0x0001;

// Report IDs. Keep in sync with kReportDescriptor below.
enum ReportId : std::uint8_t {
    kInputReportId   = 0x01,
    kOutputReportId  = 0x02,
    kFeatureReportId = 0x03,
};

// Payload sizes (bytes) NOT including the leading Report ID byte.
inline constexpr std::size_t kInputReportPayloadSize   = 16;
inline constexpr std::size_t kOutputReportPayloadSize  = 16;
inline constexpr std::size_t kFeatureReportPayloadSize = 8;

// Total wire sizes (Report ID + payload), as exchanged via Win32 HID API.
inline constexpr std::size_t kInputReportWireSize   = 1 + kInputReportPayloadSize;
inline constexpr std::size_t kOutputReportWireSize  = 1 + kOutputReportPayloadSize;
inline constexpr std::size_t kFeatureReportWireSize = 1 + kFeatureReportPayloadSize;

// Raw HID Report Descriptor bytes. Provided by report_descriptor.cpp.
extern const std::uint8_t  kReportDescriptor[];
extern const std::size_t   kReportDescriptorSize;

} // namespace vhid
