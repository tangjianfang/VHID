#include "vhid/device_inspector.hpp"
#include "vhid/descriptor_builder.hpp"

#ifdef _WIN32

#include <windows.h>
extern "C" {
#include <hidsdi.h>
}
#include <winioctl.h>

#include <algorithm>
#include <string>
#include <vector>

// IOCTL_HID_GET_REPORT_DESCRIPTOR is defined in hidclass.h (DDK).  Define it
// inline so we do not depend on the DDK headers.
#ifndef IOCTL_HID_GET_REPORT_DESCRIPTOR
#define IOCTL_HID_GET_REPORT_DESCRIPTOR \
    CTL_CODE(FILE_DEVICE_KEYBOARD, 0x100, METHOD_NEITHER, FILE_ANY_ACCESS)
#endif

namespace vhid {

namespace {

std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                                     nullptr, 0);
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                        out.data(), needed);
    return out;
}

std::string wide_to_utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                                     nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                        out.data(), needed, nullptr, nullptr);
    return out;
}

ReportInfo& find_or_add(std::vector<ReportInfo>& reports, std::uint8_t id) {
    auto it = std::find_if(reports.begin(), reports.end(),
                           [id](const ReportInfo& r) { return r.report_id == id; });
    if (it != reports.end()) return *it;
    reports.push_back(ReportInfo{});
    reports.back().report_id = id;
    return reports.back();
}

void collect_buttons(PHIDP_PREPARSED_DATA pp, HIDP_REPORT_TYPE rtype,
                     USHORT count, std::vector<ReportInfo>& reports) {
    if (count == 0) return;
    std::vector<HIDP_BUTTON_CAPS> caps(count);
    USHORT n = count;
    if (HidP_GetButtonCaps(rtype, caps.data(), &n, pp) != HIDP_STATUS_SUCCESS) return;
    for (USHORT i = 0; i < n; ++i) {
        const auto& c = caps[i];
        FieldCaps f;
        f.usage_page  = c.UsagePage;
        f.is_button   = true;
        f.is_absolute = c.IsAbsolute != 0;
        f.report_size = 1;
        if (c.IsRange) {
            f.usage_min    = c.Range.UsageMin;
            f.usage_max    = c.Range.UsageMax;
            f.report_count = static_cast<std::uint16_t>(
                c.Range.UsageMax - c.Range.UsageMin + 1);
        } else {
            f.usage_min    = c.NotRange.Usage;
            f.usage_max    = c.NotRange.Usage;
            f.report_count = 1;
        }
        f.logical_min = 0;
        f.logical_max = 1;
        find_or_add(reports, static_cast<std::uint8_t>(c.ReportID)).fields.push_back(f);
    }
}

void collect_values(PHIDP_PREPARSED_DATA pp, HIDP_REPORT_TYPE rtype,
                    USHORT count, std::vector<ReportInfo>& reports) {
    if (count == 0) return;
    std::vector<HIDP_VALUE_CAPS> caps(count);
    USHORT n = count;
    if (HidP_GetValueCaps(rtype, caps.data(), &n, pp) != HIDP_STATUS_SUCCESS) return;
    for (USHORT i = 0; i < n; ++i) {
        const auto& c = caps[i];
        FieldCaps f;
        f.usage_page    = c.UsagePage;
        f.logical_min   = c.LogicalMin;
        f.logical_max   = c.LogicalMax;
        f.physical_min  = c.PhysicalMin;
        f.physical_max  = c.PhysicalMax;
        f.report_size   = c.BitSize;
        f.report_count  = c.ReportCount;
        f.unit          = c.Units;
        f.unit_exponent = c.UnitsExp;
        f.is_button     = false;
        f.is_absolute   = c.IsAbsolute != 0;
        f.has_null      = c.HasNull != 0;
        if (c.IsRange) {
            f.usage_min = c.Range.UsageMin;
            f.usage_max = c.Range.UsageMax;
        } else {
            f.usage_min = c.NotRange.Usage;
            f.usage_max = c.NotRange.Usage;
        }
        find_or_add(reports, static_cast<std::uint8_t>(c.ReportID)).fields.push_back(f);
    }
}

bool try_get_raw_descriptor(HANDLE h, std::vector<std::uint8_t>& out) {
    // Probe with a generously sized buffer; some drivers return ERROR_INVALID_USER_BUFFER
    // if too small.
    std::vector<std::uint8_t> buf(4096);
    DWORD returned = 0;
    if (DeviceIoControl(h, IOCTL_HID_GET_REPORT_DESCRIPTOR,
                        nullptr, 0,
                        buf.data(), static_cast<DWORD>(buf.size()),
                        &returned, nullptr)) {
        if (returned > 0 && returned <= buf.size()) {
            buf.resize(returned);
            out = std::move(buf);
            return true;
        }
    }
    return false;
}

} // namespace

std::optional<DeviceProfile> inspect_device(const std::string& device_path) {
    auto wide_path = utf8_to_wide(device_path);

    HANDLE h = CreateFileW(wide_path.c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        // Many keyboards/mice are owned exclusively; retry without access.
        h = CreateFileW(wide_path.c_str(), 0,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr, OPEN_EXISTING, 0, nullptr);
    }
    if (h == INVALID_HANDLE_VALUE) return std::nullopt;

    DeviceProfile p;

    HIDD_ATTRIBUTES attrs{};
    attrs.Size = sizeof(attrs);
    if (HidD_GetAttributes(h, &attrs)) {
        p.vendor_id      = attrs.VendorID;
        p.product_id     = attrs.ProductID;
        p.version_number = attrs.VersionNumber;
    }

    wchar_t sbuf[256];
    if (HidD_GetManufacturerString(h, sbuf, sizeof(sbuf))) p.manufacturer  = wide_to_utf8(sbuf);
    if (HidD_GetProductString(h, sbuf, sizeof(sbuf)))      p.product       = wide_to_utf8(sbuf);
    if (HidD_GetSerialNumberString(h, sbuf, sizeof(sbuf))) p.serial_number = wide_to_utf8(sbuf);

    PHIDP_PREPARSED_DATA pp = nullptr;
    if (HidD_GetPreparsedData(h, &pp) && pp) {
        HIDP_CAPS caps{};
        if (HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS) {
            p.usage_page = caps.UsagePage;
            p.usage      = caps.Usage;
            collect_buttons(pp, HidP_Input,   caps.NumberInputButtonCaps,    p.input_reports);
            collect_values (pp, HidP_Input,   caps.NumberInputValueCaps,     p.input_reports);
            collect_buttons(pp, HidP_Output,  caps.NumberOutputButtonCaps,   p.output_reports);
            collect_values (pp, HidP_Output,  caps.NumberOutputValueCaps,    p.output_reports);
            collect_buttons(pp, HidP_Feature, caps.NumberFeatureButtonCaps,  p.feature_reports);
            collect_values (pp, HidP_Feature, caps.NumberFeatureValueCaps,   p.feature_reports);

            for (auto& r : p.input_reports)   r.byte_length = caps.InputReportByteLength;
            for (auto& r : p.output_reports)  r.byte_length = caps.OutputReportByteLength;
            for (auto& r : p.feature_reports) r.byte_length = caps.FeatureReportByteLength;
        }
        HidD_FreePreparsedData(pp);
    }

    if (!try_get_raw_descriptor(h, p.report_descriptor)) {
        // Fall back to reconstructing from caps.
        p.report_descriptor = build_report_descriptor(p);
    }

    CloseHandle(h);
    return p;
}

} // namespace vhid

#else // _WIN32

namespace vhid {
std::optional<DeviceProfile> inspect_device(const std::string&) { return std::nullopt; }
} // namespace vhid

#endif
