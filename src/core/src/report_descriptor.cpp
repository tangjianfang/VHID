#include "vhid/report_descriptor.hpp"

namespace vhid {

// Hand-rolled vendor-defined HID Report Descriptor.
//
// Layout (byte-for-byte stable; consumed identically by mock + driver):
//   Usage Page (Vendor-Defined 0xFF00)
//   Usage (0x01)
//   Collection (Application)
//       Report ID (0x01)            -- INPUT
//       Usage (0x02)
//       Logical Min/Max 0..255
//       Report Size 8 / Report Count 16
//       Input (Data, Var, Abs)
//
//       Report ID (0x02)            -- OUTPUT
//       Usage (0x03)
//       Report Size 8 / Report Count 16
//       Output (Data, Var, Abs)
//
//       Report ID (0x03)            -- FEATURE
//       Usage (0x04)
//       Report Size 8 / Report Count 8
//       Feature (Data, Var, Abs)
//   End Collection
const std::uint8_t kReportDescriptor[] = {
    0x06, 0x00, 0xFF,       // Usage Page (Vendor-Defined 0xFF00)
    0x09, 0x01,             // Usage (0x01)
    0xA1, 0x01,             // Collection (Application)

    // ---- Input report ----
    0x85, 0x01,             //   Report ID (1)
    0x09, 0x02,             //   Usage (0x02)
    0x15, 0x00,             //   Logical Minimum (0)
    0x26, 0xFF, 0x00,       //   Logical Maximum (255)
    0x75, 0x08,             //   Report Size (8)
    0x95, 0x10,             //   Report Count (16)
    0x81, 0x02,             //   Input (Data, Var, Abs)

    // ---- Output report ----
    0x85, 0x02,             //   Report ID (2)
    0x09, 0x03,             //   Usage (0x03)
    0x15, 0x00,             //   Logical Minimum (0)
    0x26, 0xFF, 0x00,       //   Logical Maximum (255)
    0x75, 0x08,             //   Report Size (8)
    0x95, 0x10,             //   Report Count (16)
    0x91, 0x02,             //   Output (Data, Var, Abs)

    // ---- Feature report ----
    0x85, 0x03,             //   Report ID (3)
    0x09, 0x04,             //   Usage (0x04)
    0x15, 0x00,             //   Logical Minimum (0)
    0x26, 0xFF, 0x00,       //   Logical Maximum (255)
    0x75, 0x08,             //   Report Size (8)
    0x95, 0x08,             //   Report Count (8)
    0xB1, 0x02,             //   Feature (Data, Var, Abs)

    0xC0,                   // End Collection
};

const std::size_t kReportDescriptorSize = sizeof(kReportDescriptor);

} // namespace vhid
