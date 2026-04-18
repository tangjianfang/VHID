// User-facing SDK. Wraps an IHidTransport and exposes a thin convenience API.
#pragma once

#include "vhid/hid_transport.hpp"
#include "vhid/in_process_transport.hpp"
#include "vhid/named_pipe_transport.hpp"

#include <memory>
#include <string>

namespace vhid {

// Factory: open a transport backed by the real KMDF/VHF driver via the
// control device \\.\VHidControl. Throws std::runtime_error if the driver is
// not installed/loaded.
std::unique_ptr<IHidTransport> open_driver_transport();

// Convenience facade for the host (consumer) side. Wraps any IHidTransport
// and exposes synchronous send / receive helpers used by the CLI and samples.
class Device {
public:
    explicit Device(std::unique_ptr<IHidTransport> transport);
    ~Device();

    void open()  { transport_->open(); }
    void close() { transport_->close(); }

    // Device-side helpers
    void submit_input(const InputReport& r) { transport_->submit_input(r); }
    void on_output(IHidTransport::OutputCallback cb) { transport_->on_output(std::move(cb)); }
    void on_feature_get(IHidTransport::FeatureGetter g) { transport_->on_feature_get(std::move(g)); }
    void on_feature_set(IHidTransport::FeatureSetter s) { transport_->on_feature_set(std::move(s)); }

    // Host-side helpers
    void           send_output(const OutputReport& r) { transport_->send_output(r); }
    FeatureReport  get_feature()                       { return transport_->get_feature(); }
    void           set_feature(const FeatureReport& r) { transport_->set_feature(r); }
    void           on_input(IHidTransport::InputCallback cb) { transport_->on_input(std::move(cb)); }

    IHidTransport& transport() { return *transport_; }

private:
    std::unique_ptr<IHidTransport> transport_;
};

} // namespace vhid
