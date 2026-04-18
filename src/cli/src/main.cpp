// vhid-cli — interactive injector for the virtual HID device.
//
// Usage:
//   vhid-cli mock-device [--profile <file>]   Start as named-pipe device (server side).
//   vhid-cli mock-host                        Connect as named-pipe host  (client side).
//   vhid-cli driver                           Use the real VHF driver (requires install).
//   vhid-cli capture list                     List currently-attached HID devices.
//   vhid-cli capture watch -o <file>          Wait for a new HID device, capture to JSON.
//   vhid-cli capture <index> -o <file>        Capture device by list index.
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
#include "vhid/device_enumerator.hpp"
#include "vhid/device_inspector.hpp"
#include "vhid/device_profile.hpp"

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
    std::cerr <<
        "usage:\n"
        "  vhid-cli mock-device [--profile <file>]\n"
        "  vhid-cli mock-host\n"
        "  vhid-cli driver\n"
        "  vhid-cli capture list\n"
        "  vhid-cli capture watch -o <file>\n"
        "  vhid-cli capture <index> -o <file>\n";
}

void print_profile_summary(const vhid::DeviceProfile& p) {
    std::cout << "  vendor_id      : 0x" << std::hex << std::setw(4) << std::setfill('0')
              << p.vendor_id << '\n'
              << "  product_id     : 0x" << std::setw(4) << std::setfill('0')
              << p.product_id << std::dec << '\n'
              << "  version_number : " << p.version_number << '\n'
              << "  manufacturer   : " << p.manufacturer << '\n'
              << "  product        : " << p.product << '\n'
              << "  serial_number  : " << p.serial_number << '\n'
              << "  usage_page     : 0x" << std::hex << p.usage_page << std::dec << '\n'
              << "  usage          : 0x" << std::hex << p.usage << std::dec << '\n'
              << "  input reports  : " << p.input_reports.size() << '\n'
              << "  output reports : " << p.output_reports.size() << '\n'
              << "  feature reports: " << p.feature_reports.size() << '\n'
              << "  descriptor     : " << p.report_descriptor.size() << " bytes\n";
}

int cmd_capture_list() {
    auto devices = vhid::enumerate_hid_devices();
    if (devices.empty()) {
        std::cout << "(no HID devices found)\n";
        return 0;
    }
    for (std::size_t i = 0; i < devices.size(); ++i) {
        const auto& d = devices[i];
        std::cout << '[' << i << "] VID=0x" << std::hex << std::setw(4) << std::setfill('0')
                  << d.vendor_id << " PID=0x" << std::setw(4) << std::setfill('0')
                  << d.product_id << std::dec << "  "
                  << (d.product.empty() ? "<no name>" : d.product) << '\n'
                  << "      " << d.device_path << '\n';
    }
    return 0;
}

int cmd_capture_inspect(const std::string& device_path, const std::string& out_file) {
    auto profile = vhid::inspect_device(device_path);
    if (!profile) {
        std::cerr << "failed to inspect device: " << device_path << '\n';
        return 1;
    }
    std::cout << "captured profile:\n";
    print_profile_summary(*profile);
    if (!profile->save(out_file)) {
        std::cerr << "failed to write " << out_file << '\n';
        return 1;
    }
    std::cout << "saved to " << out_file << '\n';
    return 0;
}

int cmd_capture_watch(const std::string& out_file, int timeout_seconds) {
    std::cout << "waiting up to " << timeout_seconds << "s for a new HID device... "
                                                       "(plug it in now)\n";
    std::string captured_path;
    bool found = vhid::watch_for_new_device(
        [&](const vhid::HidDeviceInfo& dev) {
            std::cout << "detected: VID=0x" << std::hex << std::setw(4) << std::setfill('0')
                      << dev.vendor_id << " PID=0x" << std::setw(4) << std::setfill('0')
                      << dev.product_id << std::dec << "  "
                      << (dev.product.empty() ? "<no name>" : dev.product) << '\n';
            captured_path = dev.device_path;
            return true;
        }, timeout_seconds);
    if (!found) {
        std::cerr << "timeout: no new device\n";
        return 1;
    }
    return cmd_capture_inspect(captured_path, out_file);
}

int cmd_capture(int argc, char** argv) {
    // argv[0] = "capture"; argv[1..] = sub-args
    if (argc < 2) {
        std::cerr << "capture requires a sub-command (list|watch|<index>)\n";
        return 2;
    }
    std::string sub = argv[1];
    if (sub == "list") return cmd_capture_list();

    auto find_out_file = [&]() -> std::string {
        for (int i = 1; i < argc - 1; ++i) {
            if (std::string(argv[i]) == "-o") return argv[i + 1];
        }
        return {};
    };

    std::string out_file = find_out_file();
    if (out_file.empty()) {
        std::cerr << "-o <file> is required for capture\n";
        return 2;
    }

    if (sub == "watch") {
        return cmd_capture_watch(out_file, /*timeout_seconds=*/60);
    }

    // Otherwise treat sub as numeric index into enumerate_hid_devices().
    try {
        std::size_t index = std::stoul(sub);
        auto devices = vhid::enumerate_hid_devices();
        if (index >= devices.size()) {
            std::cerr << "index out of range (have " << devices.size() << " devices)\n";
            return 1;
        }
        return cmd_capture_inspect(devices[index].device_path, out_file);
    } catch (const std::exception& e) {
        std::cerr << "bad capture sub-command: " << sub << " (" << e.what() << ")\n";
        return 2;
    }
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

    if (mode == "capture") {
        return cmd_capture(argc - 1, argv + 1);
    }

    if (mode == "mock-device") {
        // Optional --profile <file>: load and display the captured profile that
        // this mock device is meant to emulate.  Wire-level descriptor injection
        // is a future iteration; for now we surface the profile so the user can
        // verify their JSON is well-formed and what would be cloned.
        for (int i = 2; i + 1 < argc; ++i) {
            if (std::string(argv[i]) == "--profile") {
                auto p = vhid::DeviceProfile::load(argv[i + 1]);
                if (!p) {
                    std::cerr << "failed to load profile: " << argv[i + 1] << '\n';
                    return 1;
                }
                std::cout << "loaded profile from " << argv[i + 1] << ":\n";
                print_profile_summary(*p);
            }
        }
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
