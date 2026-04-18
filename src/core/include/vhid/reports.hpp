#pragma once

#include "vhid/report_descriptor.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace vhid {

// Strongly-typed report POD wrappers. All store the wire form (Report ID + payload)
// so they can be passed straight to ReadFile / WriteFile / HidD_GetFeature.
template <std::uint8_t Id, std::size_t PayloadSize>
struct Report {
    static constexpr std::uint8_t  kId          = Id;
    static constexpr std::size_t   kPayloadSize = PayloadSize;
    static constexpr std::size_t   kWireSize    = 1 + PayloadSize;

    std::array<std::uint8_t, kWireSize> wire{};

    Report() { wire[0] = kId; }

    std::uint8_t* payload() noexcept { return wire.data() + 1; }
    const std::uint8_t* payload() const noexcept { return wire.data() + 1; }

    void set_payload(const void* data, std::size_t size) {
        if (size > kPayloadSize) throw std::length_error("payload too large");
        std::memcpy(payload(), data, size);
        if (size < kPayloadSize) {
            std::memset(payload() + size, 0, kPayloadSize - size);
        }
    }

    bool operator==(const Report& other) const noexcept { return wire == other.wire; }
    bool operator!=(const Report& other) const noexcept { return !(*this == other); }
};

using InputReport   = Report<kInputReportId,   kInputReportPayloadSize>;
using OutputReport  = Report<kOutputReportId,  kOutputReportPayloadSize>;
using FeatureReport = Report<kFeatureReportId, kFeatureReportPayloadSize>;

// Helpers: build a typed report from a raw wire buffer (validates Report ID and size).
InputReport   parse_input_report(const std::uint8_t* data, std::size_t size);
OutputReport  parse_output_report(const std::uint8_t* data, std::size_t size);
FeatureReport parse_feature_report(const std::uint8_t* data, std::size_t size);

} // namespace vhid
