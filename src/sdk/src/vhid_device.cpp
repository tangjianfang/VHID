#include "vhid/vhid_device.hpp"

namespace vhid {

Device::Device(std::unique_ptr<IHidTransport> transport)
    : transport_(std::move(transport)) {}

Device::~Device() {
    if (transport_) transport_->close();
}

} // namespace vhid
