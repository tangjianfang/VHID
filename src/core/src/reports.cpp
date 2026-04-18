#include "vhid/reports.hpp"

namespace vhid {

namespace {

template <typename R>
R parse_impl(const std::uint8_t* data, std::size_t size) {
    if (size != R::kWireSize) {
        throw std::length_error("report wire size mismatch");
    }
    if (data[0] != R::kId) {
        throw std::invalid_argument("report id mismatch");
    }
    R r;
    std::memcpy(r.wire.data(), data, R::kWireSize);
    return r;
}

} // namespace

InputReport parse_input_report(const std::uint8_t* data, std::size_t size) {
    return parse_impl<InputReport>(data, size);
}
OutputReport parse_output_report(const std::uint8_t* data, std::size_t size) {
    return parse_impl<OutputReport>(data, size);
}
FeatureReport parse_feature_report(const std::uint8_t* data, std::size_t size) {
    return parse_impl<FeatureReport>(data, size);
}

} // namespace vhid
