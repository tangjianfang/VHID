// IHidTransport — abstraction shared by mock + real (driver-backed) transports.
// The SDK selects an implementation at runtime.
#pragma once

#include "vhid/reports.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace vhid {

// Direction conventions:
//   - "Input"   = device -> host    (device-side calls SubmitInput; host-side reads via OnInput)
//   - "Output"  = host   -> device  (host-side calls SendOutput;   device-side observes via OnOutput)
//   - "Feature" = bidirectional    (host can Get or Set; device serves the value)
class IHidTransport {
public:
    virtual ~IHidTransport() = default;

    // Lifecycle
    virtual void open()  = 0;
    virtual void close() = 0;

    // Device-side: push an Input report so the host eventually observes it.
    virtual void submit_input(const InputReport& report) = 0;

    // Host-side: send an Output report toward the device.
    virtual void send_output(const OutputReport& report) = 0;

    // Host-side: get/set Feature report (synchronous, blocking).
    virtual FeatureReport get_feature() = 0;
    virtual void          set_feature(const FeatureReport& report) = 0;

    // Host-side: subscribe to Input reports (invoked from a transport-internal thread).
    using InputCallback = std::function<void(const InputReport&)>;
    virtual void on_input(InputCallback cb) = 0;

    // Device-side: subscribe to Output reports.
    using OutputCallback = std::function<void(const OutputReport&)>;
    virtual void on_output(OutputCallback cb) = 0;

    // Device-side: install handlers for Feature get/set.
    using FeatureGetter = std::function<FeatureReport()>;
    using FeatureSetter = std::function<void(const FeatureReport&)>;
    virtual void on_feature_get(FeatureGetter getter) = 0;
    virtual void on_feature_set(FeatureSetter setter) = 0;
};

} // namespace vhid
