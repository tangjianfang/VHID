// vhid-cli — interactive injector for the virtual HID device.
//
// Usage:
//   vhid-cli mock-device            Start as named-pipe device (server side).
//   vhid-cli mock-host              Connect as named-pipe host  (client side).
//   vhid-cli driver                 Use the real VHF driver (requires install).
//
// Once attached, type one of the commands followed by Enter:
//   in  <hex bytes>     submit an Input report   (device-side only)
//   out <hex bytes>     send  an Output report   (host-side only)
//   getf                get   a Feature report   (host-side only)
//   setf <hex bytes>    set   a Feature report   (host-side only)
//   sleep <ms>          pause input loop for <ms> milliseconds (both sides)
//   quit
//
// <hex bytes> are space-separated, e.g.:  in 11 22 33 44

#include "vhid/vhid_device.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::vector<std::uint8_t> parse_hex(const std::string& s) {
    std::vector<std::uint8_t> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) {
        out.push_back(static_cast<std::uint8_t>(std::stoul(tok, nullptr, 16)));
    }
    return out;
}

std::string to_hex(const std::uint8_t* data, std::size_t n) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < n; ++i) {
        if (i) oss << ' ';
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return oss.str();
}

void print_usage() {
    std::cerr << "usage: vhid-cli {mock-device|mock-host|driver}\n";
}

int run(std::unique_ptr<vhid::IHidTransport> transport, bool host_side) {
    vhid::Device dev(std::move(transport));

    if (host_side) {
        dev.on_input([](const vhid::InputReport& r) {
            std::cout << "[INPUT ] " << to_hex(r.wire.data(), r.wire.size()) << '\n';
        });
    } else {
        dev.on_output([](const vhid::OutputReport& r) {
            std::cout << "[OUTPUT] " << to_hex(r.wire.data(), r.wire.size()) << '\n';
        });
        dev.on_feature_get([] {
            std::cout << "[FEAT-GET]\n";
            return vhid::FeatureReport{};
        });
        dev.on_feature_set([](const vhid::FeatureReport& r) {
            std::cout << "[FEAT-SET] " << to_hex(r.wire.data(), r.wire.size()) << '\n';
        });
    }

    try {
        dev.open();
    } catch (const std::exception& e) {
        std::cerr << "open failed: " << e.what() << '\n';
        return 1;
    }
    std::cout << "ready. type 'quit' to exit.\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        std::string rest;
        std::getline(iss, rest);

        try {
            if (cmd == "quit") break;
            else if (cmd == "sleep") {
                int ms = 0;
                std::istringstream(rest) >> ms;
                if (ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            }
            else if (cmd == "in" && !host_side) {
                vhid::InputReport r;
                auto bytes = parse_hex(rest);
                r.set_payload(bytes.data(), bytes.size());
                dev.submit_input(r);
            }
            else if (cmd == "out" && host_side) {
                vhid::OutputReport r;
                auto bytes = parse_hex(rest);
                r.set_payload(bytes.data(), bytes.size());
                dev.send_output(r);
            }
            else if (cmd == "getf" && host_side) {
                auto r = dev.get_feature();
                std::cout << "[FEAT  ] " << to_hex(r.wire.data(), r.wire.size()) << '\n';
            }
            else if (cmd == "setf" && host_side) {
                vhid::FeatureReport r;
                auto bytes = parse_hex(rest);
                r.set_payload(bytes.data(), bytes.size());
                dev.set_feature(r);
            }
            else {
                std::cerr << "unknown or wrong-side command: " << cmd << '\n';
            }
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << '\n';
        }
    }

    dev.close();
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { print_usage(); return 2; }
    std::string mode = argv[1];

    if (mode == "mock-device") {
        return run(std::make_unique<vhid::NamedPipeMockTransport>(
                       vhid::NamedPipeMockTransport::Role::Device),
                   /*host_side=*/false);
    }
    if (mode == "mock-host") {
        return run(std::make_unique<vhid::NamedPipeMockTransport>(
                       vhid::NamedPipeMockTransport::Role::Host),
                   /*host_side=*/true);
    }
    if (mode == "driver") {
        try {
            return run(vhid::open_driver_transport(), /*host_side=*/false);
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            return 1;
        }
    }
    print_usage();
    return 2;
}
