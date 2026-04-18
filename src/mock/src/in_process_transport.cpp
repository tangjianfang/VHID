#include "vhid/in_process_transport.hpp"

#include <mutex>

namespace vhid {

struct InProcessMockTransport::State {
    std::mutex mutex;
    bool open = false;

    InputCallback   on_input;
    OutputCallback  on_output;
    FeatureGetter   feature_getter;
    FeatureSetter   feature_setter;

    FeatureReport last_feature{};  // default value if no getter installed
};

InProcessMockTransport::InProcessMockTransport()
    : state_(std::make_unique<State>()) {}

InProcessMockTransport::~InProcessMockTransport() = default;

void InProcessMockTransport::open() {
    std::lock_guard<std::mutex> lk(state_->mutex);
    state_->open = true;
}

void InProcessMockTransport::close() {
    std::lock_guard<std::mutex> lk(state_->mutex);
    state_->open = false;
}

void InProcessMockTransport::submit_input(const InputReport& report) {
    InputCallback cb;
    {
        std::lock_guard<std::mutex> lk(state_->mutex);
        if (!state_->open) return;
        cb = state_->on_input;
    }
    if (cb) cb(report);
}

void InProcessMockTransport::send_output(const OutputReport& report) {
    OutputCallback cb;
    {
        std::lock_guard<std::mutex> lk(state_->mutex);
        if (!state_->open) return;
        cb = state_->on_output;
    }
    if (cb) cb(report);
}

FeatureReport InProcessMockTransport::get_feature() {
    FeatureGetter getter;
    {
        std::lock_guard<std::mutex> lk(state_->mutex);
        getter = state_->feature_getter;
        if (!getter) return state_->last_feature;
    }
    return getter();
}

void InProcessMockTransport::set_feature(const FeatureReport& report) {
    FeatureSetter setter;
    {
        std::lock_guard<std::mutex> lk(state_->mutex);
        state_->last_feature = report;
        setter = state_->feature_setter;
    }
    if (setter) setter(report);
}

void InProcessMockTransport::on_input(InputCallback cb) {
    std::lock_guard<std::mutex> lk(state_->mutex);
    state_->on_input = std::move(cb);
}

void InProcessMockTransport::on_output(OutputCallback cb) {
    std::lock_guard<std::mutex> lk(state_->mutex);
    state_->on_output = std::move(cb);
}

void InProcessMockTransport::on_feature_get(FeatureGetter getter) {
    std::lock_guard<std::mutex> lk(state_->mutex);
    state_->feature_getter = std::move(getter);
}

void InProcessMockTransport::on_feature_set(FeatureSetter setter) {
    std::lock_guard<std::mutex> lk(state_->mutex);
    state_->feature_setter = std::move(setter);
}

} // namespace vhid
