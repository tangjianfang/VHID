// Sample consumer: enumerates HID devices via SetupDi, opens the virtual VHID
// device by VID/PID (defaults from vhid::kDefaultVendorId / kDefaultProductId),
// reads Input reports, prints them, and sends a single Output report.
//
// This program is the "no-mock" target audience: it is the kind of code your
// real system uses today. When the VHF driver from driver/vhid is installed,
// running this against vhid-cli driver should print the injected reports.

#include "vhid/report_descriptor.hpp"
#include "vhid/reports.hpp"

#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

std::wstring find_device_path(USHORT vid, USHORT pid) {
    GUID hid_guid;
    HidD_GetHidGuid(&hid_guid);

    HDEVINFO info = SetupDiGetClassDevsW(&hid_guid, nullptr, nullptr,
                                         DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE) return {};

    SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize = sizeof(iface);
    std::wstring found;
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(info, nullptr, &hid_guid, i, &iface); ++i) {
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetailW(info, &iface, nullptr, 0, &needed, nullptr);
        std::vector<std::uint8_t> buf(needed);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(info, &iface, detail, needed, nullptr, nullptr))
            continue;

        HANDLE h = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attrs{}; attrs.Size = sizeof(attrs);
        if (HidD_GetAttributes(h, &attrs) && attrs.VendorID == vid && attrs.ProductID == pid) {
            found = detail->DevicePath;
            CloseHandle(h);
            break;
        }
        CloseHandle(h);
    }
    SetupDiDestroyDeviceInfoList(info);
    return found;
}

} // namespace

int wmain() {
    auto path = find_device_path(vhid::kDefaultVendorId, vhid::kDefaultProductId);
    if (path.empty()) {
        std::wprintf(L"VHID device VID=%04X PID=%04X not found. "
                     L"Install the driver and try again.\n",
                     vhid::kDefaultVendorId, vhid::kDefaultProductId);
        return 1;
    }
    std::wprintf(L"opening %ls\n", path.c_str());

    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        std::wprintf(L"CreateFileW failed: %lu\n", GetLastError());
        return 1;
    }

    // Read 5 input reports.
    for (int i = 0; i < 5; ++i) {
        std::uint8_t buf[vhid::kInputReportWireSize] = {};
        DWORD got = 0;
        if (!ReadFile(h, buf, sizeof(buf), &got, nullptr)) {
            std::wprintf(L"ReadFile failed: %lu\n", GetLastError());
            break;
        }
        std::printf("input[%d] (%lu bytes):", i, got);
        for (DWORD j = 0; j < got; ++j) std::printf(" %02X", buf[j]);
        std::printf("\n");
    }

    // Send one output report.
    vhid::OutputReport out;
    std::uint8_t payload[] = {0xCA, 0xFE, 0xBA, 0xBE};
    out.set_payload(payload, sizeof(payload));
    DWORD wrote = 0;
    if (!WriteFile(h, out.wire.data(), static_cast<DWORD>(out.wire.size()), &wrote, nullptr)) {
        std::wprintf(L"WriteFile failed: %lu\n", GetLastError());
    } else {
        std::printf("wrote output report (%lu bytes)\n", wrote);
    }

    CloseHandle(h);
    return 0;
}
