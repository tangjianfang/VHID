#pragma once

#include "vhid/hid_transport.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace vhid {

// Cross-process transport over a Windows named pipe.
//
// Wire framing: each message is [u8 opcode][u16 LE length][payload bytes].
// Opcodes:
//   0x01 INPUT_REPORT     payload = full input wire bytes      (device -> host)
//   0x02 OUTPUT_REPORT    payload = full output wire bytes     (host   -> device)
//   0x03 FEATURE_GET_REQ  payload = empty                      (host   -> device)
//   0x04 FEATURE_GET_RSP  payload = full feature wire bytes    (device -> host)
//   0x05 FEATURE_SET      payload = full feature wire bytes    (host   -> device)
//
// Two roles share the same class:
//   Role::Device -- creates the pipe (server), waits for one host to connect.
//   Role::Host   -- connects to the existing pipe.
class NamedPipeMockTransport final : public IHidTransport {
public:
    enum class Role { Device, Host };

    NamedPipeMockTransport(Role role, std::wstring pipe_name = L"\\\\.\\pipe\\vhid_mock");
    ~NamedPipeMockTransport() override;

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
