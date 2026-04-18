#include "vhid/device_enumerator.hpp"

#include <chrono>
#include <set>
#include <thread>

#ifdef _WIN32

#include <windows.h>
#include <setupapi.h>
extern "C" {
#include <hidsdi.h>
}

#include <memory>
#include <string>
#include <vector>

namespace vhid {

namespace {

std::string wide_to_utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                                     nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                        out.data(), needed, nullptr, nullptr);
    return out;
}

void fill_strings_and_attrs(const std::wstring& wide_path, HidDeviceInfo& info) {
    HANDLE h = CreateFileW(wide_path.c_str(), 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;

    HIDD_ATTRIBUTES attrs{};
    attrs.Size = sizeof(attrs);
    if (HidD_GetAttributes(h, &attrs)) {
        info.vendor_id      = attrs.VendorID;
        info.product_id     = attrs.ProductID;
        info.version_number = attrs.VersionNumber;
    }

    wchar_t buf[256];
    if (HidD_GetProductString(h, buf, sizeof(buf))) {
        info.product = wide_to_utf8(buf);
    }
    if (HidD_GetManufacturerString(h, buf, sizeof(buf))) {
        info.manufacturer = wide_to_utf8(buf);
    }
    CloseHandle(h);
}

} // namespace

std::vector<HidDeviceInfo> enumerate_hid_devices() {
    std::vector<HidDeviceInfo> result;

    GUID hid_guid;
    HidD_GetHidGuid(&hid_guid);

    HDEVINFO dev_info = SetupDiGetClassDevsW(
        &hid_guid, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (dev_info == INVALID_HANDLE_VALUE) return result;

    SP_DEVICE_INTERFACE_DATA iface_data{};
    iface_data.cbSize = sizeof(iface_data);

    for (DWORD index = 0;
         SetupDiEnumDeviceInterfaces(dev_info, nullptr, &hid_guid, index, &iface_data);
         ++index) {
        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(dev_info, &iface_data,
                                         nullptr, 0, &required, nullptr);
        if (required == 0) continue;

        std::vector<std::uint8_t> buf(required);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(dev_info, &iface_data,
                                              detail, required, nullptr, nullptr)) {
            continue;
        }

        HidDeviceInfo info;
        info.device_path = wide_to_utf8(detail->DevicePath);
        fill_strings_and_attrs(detail->DevicePath, info);
        result.push_back(std::move(info));
    }

    SetupDiDestroyDeviceInfoList(dev_info);
    return result;
}

bool watch_for_new_device(
    std::function<bool(const HidDeviceInfo&)> on_new_device,
    int timeout_seconds) {

    std::set<std::string> baseline;
    for (const auto& d : enumerate_hid_devices()) baseline.insert(d.device_path);

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds(timeout_seconds);

    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        auto current = enumerate_hid_devices();
        for (const auto& dev : current) {
            if (baseline.count(dev.device_path) == 0) {
                if (on_new_device(dev)) return true;
                baseline.insert(dev.device_path);
            }
        }
    }
    return false;
}

} // namespace vhid

#else // _WIN32

namespace vhid {
std::vector<HidDeviceInfo> enumerate_hid_devices() { return {}; }
bool watch_for_new_device(std::function<bool(const HidDeviceInfo&)>, int) { return false; }
} // namespace vhid

#endif
