#pragma once

#include "vhid/hid_transport.hpp"

#include <memory>
#include <mutex>

namespace vhid {

// Single-process transport: device side and host side live in the same process and
// communicate through a shared queue + direct callback. Ideal for unit tests where
// the production code can be wired against IHidTransport directly.
class InProcessMockTransport final : public IHidTransport {
public:
    InProcessMockTransport();
    ~InProcessMockTransport() override;

    void open() override;
    void close() override;

    void submit_input(const InputReport& report) override;
    void send_output(const OutputReport& report) override;
    FeatureReport get_feature() override;
    void          set_feature(const FeatureReport& report) override;

    void on_input(InputCallback cb) override;
    void on_output(OutputCallback cb) override;
    void on_feature_get(FeatureGetter getter) override;
    void on_feature_set(FeatureSetter setter) override;

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace vhid
