#include "vhid/named_pipe_transport.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace vhid {

namespace {

enum Opcode : std::uint8_t {
    kOpInput        = 0x01,
    kOpOutput       = 0x02,
    kOpFeatureGetQ  = 0x03,
    kOpFeatureGetR  = 0x04,
    kOpFeatureSet   = 0x05,
};

#ifdef _WIN32
struct PipeHandle {
    HANDLE h = INVALID_HANDLE_VALUE;
    ~PipeHandle() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
};

// Wait helper for an overlapped operation. Returns true on success.
static bool wait_overlapped(HANDLE h, OVERLAPPED& ov, DWORD& transferred) {
    DWORD err = GetLastError();
    if (err == ERROR_IO_PENDING) {
        if (!GetOverlappedResult(h, &ov, &transferred, TRUE)) return false;
        return true;
    }
    // Operation completed synchronously (success path on some pipe states).
    return true;
}

bool write_all(HANDLE h, const void* data, DWORD size) {
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    DWORD written = 0;
    BOOL ok = WriteFile(h, data, size, &written, &ov);
    if (!ok) {
        if (!wait_overlapped(h, ov, written)) { CloseHandle(ov.hEvent); return false; }
    }
    CloseHandle(ov.hEvent);
    return written == size;
}

bool read_all(HANDLE h, void* data, DWORD size) {
    auto* p = static_cast<std::uint8_t*>(data);
    DWORD got = 0;
    while (got < size) {
        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        DWORD chunk = 0;
        BOOL ok = ReadFile(h, p + got, size - got, &chunk, &ov);
        if (!ok) {
            if (!wait_overlapped(h, ov, chunk)) { CloseHandle(ov.hEvent); return false; }
        }
        CloseHandle(ov.hEvent);
        if (chunk == 0) return false;
        got += chunk;
    }
    return true;
}

bool write_frame(HANDLE h, std::uint8_t op, const std::uint8_t* payload, std::uint16_t len) {
    std::uint8_t header[3] = { op, static_cast<std::uint8_t>(len & 0xFF),
                               static_cast<std::uint8_t>((len >> 8) & 0xFF) };
    if (!write_all(h, header, 3)) return false;
    if (len > 0 && !write_all(h, payload, len)) return false;
    return true;
}

bool read_frame(HANDLE h, std::uint8_t& op, std::vector<std::uint8_t>& payload) {
    std::uint8_t header[3];
    if (!read_all(h, header, 3)) return false;
    op = header[0];
    std::uint16_t len = static_cast<std::uint16_t>(header[1] | (header[2] << 8));
    payload.resize(len);
    if (len > 0 && !read_all(h, payload.data(), len)) return false;
    return true;
}
#endif

} // namespace

struct NamedPipeMockTransport::State {
    Role role;
    std::wstring pipe_name;

#ifdef _WIN32
    HANDLE pipe = INVALID_HANDLE_VALUE;
#endif
    std::atomic<bool> running{false};
    std::thread reader;
    std::mutex write_mutex;

    // Callbacks
    std::mutex cb_mutex;
    InputCallback   on_input;
    OutputCallback  on_output;
    FeatureGetter   feature_getter;
    FeatureSetter   feature_setter;

    // Feature get RPC (host side waits on this)
    std::mutex                 feature_mutex;
    std::condition_variable    feature_cv;
    bool                       feature_pending = false;
    FeatureReport              feature_response{};
};

NamedPipeMockTransport::NamedPipeMockTransport(Role role, std::wstring pipe_name)
    : state_(std::make_unique<State>()) {
    state_->role = role;
    state_->pipe_name = std::move(pipe_name);
}

NamedPipeMockTransport::~NamedPipeMockTransport() {
    close();
}

void NamedPipeMockTransport::open() {
#ifdef _WIN32
    if (state_->running.load()) return;

    if (state_->role == Role::Device) {
        state_->pipe = CreateNamedPipeW(
            state_->pipe_name.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,                           // single instance
            64 * 1024, 64 * 1024,
            0, nullptr);
        if (state_->pipe == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("CreateNamedPipeW failed");
        }
        // Wait for the host to connect (use overlapped + wait so pipe stays
        // FILE_FLAG_OVERLAPPED enabled for subsequent I/O).
        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        BOOL ok = ConnectNamedPipe(state_->pipe, &ov);
        if (!ok) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                WaitForSingleObject(ov.hEvent, INFINITE);
            } else if (err != ERROR_PIPE_CONNECTED) {
                CloseHandle(ov.hEvent);
                CloseHandle(state_->pipe);
                state_->pipe = INVALID_HANDLE_VALUE;
                throw std::runtime_error("ConnectNamedPipe failed");
            }
        }
        CloseHandle(ov.hEvent);
    } else {
        // Host: try to open the existing pipe. Caller is expected to ensure
        // the device side has called open() first.
        state_->pipe = CreateFileW(
            state_->pipe_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if (state_->pipe == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("CreateFileW (named pipe) failed");
        }
    }

    state_->running.store(true);
    state_->reader = std::thread([this] {
        std::uint8_t op;
        std::vector<std::uint8_t> payload;
        while (state_->running.load()) {
            if (!read_frame(state_->pipe, op, payload)) break;

            switch (op) {
                case kOpInput: {
                    InputCallback cb;
                    { std::lock_guard<std::mutex> lk(state_->cb_mutex); cb = state_->on_input; }
                    if (cb && payload.size() == InputReport::kWireSize) {
                        cb(parse_input_report(payload.data(), payload.size()));
                    }
                    break;
                }
                case kOpOutput: {
                    OutputCallback cb;
                    { std::lock_guard<std::mutex> lk(state_->cb_mutex); cb = state_->on_output; }
                    if (cb && payload.size() == OutputReport::kWireSize) {
                        cb(parse_output_report(payload.data(), payload.size()));
                    }
                    break;
                }
                case kOpFeatureGetQ: {
                    // Device side: serve the request.
                    FeatureGetter g;
                    { std::lock_guard<std::mutex> lk(state_->cb_mutex); g = state_->feature_getter; }
                    FeatureReport r = g ? g() : FeatureReport{};
                    std::lock_guard<std::mutex> lk(state_->write_mutex);
                    write_frame(state_->pipe, kOpFeatureGetR, r.wire.data(),
                                static_cast<std::uint16_t>(r.wire.size()));
                    break;
                }
                case kOpFeatureGetR: {
                    // Host side: deliver to waiting get_feature() caller.
                    if (payload.size() == FeatureReport::kWireSize) {
                        std::lock_guard<std::mutex> lk(state_->feature_mutex);
                        state_->feature_response =
                            parse_feature_report(payload.data(), payload.size());
                        state_->feature_pending = false;
                        state_->feature_cv.notify_all();
                    }
                    break;
                }
                case kOpFeatureSet: {
                    FeatureSetter s;
                    { std::lock_guard<std::mutex> lk(state_->cb_mutex); s = state_->feature_setter; }
                    if (payload.size() == FeatureReport::kWireSize) {
                        FeatureReport r =
                            parse_feature_report(payload.data(), payload.size());
                        if (s) s(r);
                    }
                    break;
                }
                default: break;
            }
        }
    });
#else
    throw std::runtime_error("NamedPipeMockTransport requires Windows");
#endif
}

void NamedPipeMockTransport::close() {
#ifdef _WIN32
    if (!state_->running.exchange(false)) {
        if (state_->pipe != INVALID_HANDLE_VALUE) {
            CloseHandle(state_->pipe);
            state_->pipe = INVALID_HANDLE_VALUE;
        }
        return;
    }
    if (state_->pipe != INVALID_HANDLE_VALUE) {
        if (state_->role == Role::Device) DisconnectNamedPipe(state_->pipe);
        CloseHandle(state_->pipe);
        state_->pipe = INVALID_HANDLE_VALUE;
    }
    if (state_->reader.joinable()) state_->reader.join();
    // Wake any waiter on get_feature().
    {
        std::lock_guard<std::mutex> lk(state_->feature_mutex);
        state_->feature_pending = false;
    }
    state_->feature_cv.notify_all();
#endif
}

void NamedPipeMockTransport::submit_input(const InputReport& report) {
#ifdef _WIN32
    std::lock_guard<std::mutex> lk(state_->write_mutex);
    write_frame(state_->pipe, kOpInput, report.wire.data(),
                static_cast<std::uint16_t>(report.wire.size()));
#else
    (void)report;
#endif
}

void NamedPipeMockTransport::send_output(const OutputReport& report) {
#ifdef _WIN32
    std::lock_guard<std::mutex> lk(state_->write_mutex);
    write_frame(state_->pipe, kOpOutput, report.wire.data(),
                static_cast<std::uint16_t>(report.wire.size()));
#else
    (void)report;
#endif
}

FeatureReport NamedPipeMockTransport::get_feature() {
#ifdef _WIN32
    {
        std::lock_guard<std::mutex> lk(state_->feature_mutex);
        state_->feature_pending = true;
    }
    {
        std::lock_guard<std::mutex> lk(state_->write_mutex);
        write_frame(state_->pipe, kOpFeatureGetQ, nullptr, 0);
    }
    std::unique_lock<std::mutex> lk(state_->feature_mutex);
    state_->feature_cv.wait(lk, [&] { return !state_->feature_pending; });
    return state_->feature_response;
#else
    return {};
#endif
}

void NamedPipeMockTransport::set_feature(const FeatureReport& report) {
#ifdef _WIN32
    std::lock_guard<std::mutex> lk(state_->write_mutex);
    write_frame(state_->pipe, kOpFeatureSet, report.wire.data(),
                static_cast<std::uint16_t>(report.wire.size()));
#else
    (void)report;
#endif
}

void NamedPipeMockTransport::on_input(InputCallback cb) {
    std::lock_guard<std::mutex> lk(state_->cb_mutex);
    state_->on_input = std::move(cb);
}
void NamedPipeMockTransport::on_output(OutputCallback cb) {
    std::lock_guard<std::mutex> lk(state_->cb_mutex);
    state_->on_output = std::move(cb);
}
void NamedPipeMockTransport::on_feature_get(FeatureGetter g) {
    std::lock_guard<std::mutex> lk(state_->cb_mutex);
    state_->feature_getter = std::move(g);
}
void NamedPipeMockTransport::on_feature_set(FeatureSetter s) {
    std::lock_guard<std::mutex> lk(state_->cb_mutex);
    state_->feature_setter = std::move(s);
}

} // namespace vhid
