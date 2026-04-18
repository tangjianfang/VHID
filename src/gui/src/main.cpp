// VHID Control Center — Razer-styled Win32 GUI.
//
// One-window dashboard around the vhid_capture / vhid_sdk / vhid_mock libraries:
//   * Enumerate / monitor HID devices
//   * Capture a device into a JSON profile
//   * Load profile + start a named-pipe mock device
//   * Inject Input / Output / Feature reports interactively
//
// Pure Win32 + Common Controls (no MFC, no Qt). Razer color palette via
// owner-draw buttons and CTLCOLOR* handlers.

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <uxtheme.h>

#include <atomic>
#include <chrono>
#include <iomanip>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "vhid/device_enumerator.hpp"
#include "vhid/device_inspector.hpp"
#include "vhid/device_profile.hpp"
#include "vhid/named_pipe_transport.hpp"
#include "vhid/vhid_device.hpp"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "uxtheme.lib")

// ---- Razer palette --------------------------------------------------------

namespace razer {
constexpr COLORREF kBackground = RGB(0x0F, 0x0F, 0x0F);
constexpr COLORREF kPanel      = RGB(0x1A, 0x1A, 0x1A);
constexpr COLORREF kPanelAlt   = RGB(0x14, 0x14, 0x14);
constexpr COLORREF kBorder     = RGB(0x2A, 0x2A, 0x2A);
constexpr COLORREF kAccent     = RGB(0x44, 0xD6, 0x2C);
constexpr COLORREF kAccentHi   = RGB(0x6E, 0xE6, 0x4F);
constexpr COLORREF kAccentLo   = RGB(0x2E, 0x9E, 0x1E);
constexpr COLORREF kText       = RGB(0xE0, 0xE0, 0xE0);
constexpr COLORREF kTextDim    = RGB(0x88, 0x88, 0x88);
constexpr COLORREF kDanger     = RGB(0xE0, 0x40, 0x40);
}

// ---- Control IDs ----------------------------------------------------------

enum : int {
    IDC_BTN_REFRESH = 1001,
    IDC_BTN_WATCH,
    IDC_BTN_CAPTURE,
    IDC_BTN_SAVE,
    IDC_BTN_LOAD,
    IDC_BTN_MOCK_START,
    IDC_BTN_MOCK_STOP,
    IDC_BTN_INJECT_INPUT,
    IDC_BTN_INJECT_FEATURE_GET_REPLY,
    IDC_BTN_CLEAR_LOG,
    IDC_LIST_DEVICES = 2001,
    IDC_EDIT_DETAILS,
    IDC_EDIT_INPUT_HEX,
    IDC_EDIT_FEATURE_HEX,
    IDC_EDIT_LOG,
    IDC_STATIC_STATUS,
};

constexpr UINT WM_APP_LOG          = WM_APP + 1;
constexpr UINT WM_APP_DEVICE_FOUND = WM_APP + 2;

// ---- Global app state -----------------------------------------------------

struct AppState {
    HINSTANCE hInst = nullptr;
    HWND      hMain = nullptr;
    HFONT     hFont = nullptr;
    HFONT     hFontBold = nullptr;
    HFONT     hFontMono = nullptr;
    HBRUSH    brBackground = nullptr;
    HBRUSH    brPanel = nullptr;
    HBRUSH    brPanelAlt = nullptr;
    HBRUSH    brBorder = nullptr;

    HWND hListDevices = nullptr;
    HWND hEditDetails = nullptr;
    HWND hEditInputHex = nullptr;
    HWND hEditFeatureHex = nullptr;
    HWND hEditLog = nullptr;
    HWND hStatus = nullptr;

    std::vector<vhid::HidDeviceInfo> devices;
    std::optional<vhid::DeviceProfile> currentProfile;

    // Mock device state
    std::unique_ptr<vhid::Device> mockDevice;
    std::atomic<bool> mockRunning{false};

    // Watch-thread state
    std::atomic<bool> watchActive{false};
    std::thread       watchThread;
};

static AppState g_app;

// ---- Helpers --------------------------------------------------------------

static std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

static std::string wide_to_utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

static std::vector<std::uint8_t> parse_hex(const std::string& s) {
    std::vector<std::uint8_t> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) {
        try {
            out.push_back((std::uint8_t)std::stoul(tok, nullptr, 16));
        } catch (...) {}
    }
    return out;
}

static std::string bytes_to_hex(const std::uint8_t* p, std::size_t n) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < n; ++i) {
        if (i) oss << ' ';
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)p[i];
    }
    return oss.str();
}

static void log_line(const std::string& s) {
    // Marshal to UI thread.
    auto* heap = new std::string(s);
    PostMessage(g_app.hMain, WM_APP_LOG, 0, (LPARAM)heap);
}

static void set_status(const std::wstring& w) {
    SetWindowTextW(g_app.hStatus, w.c_str());
}

static void append_log_ui(const std::string& s) {
    auto w = utf8_to_wide(s);
    w += L"\r\n";
    int len = GetWindowTextLengthW(g_app.hEditLog);
    SendMessageW(g_app.hEditLog, EM_SETSEL, len, len);
    SendMessageW(g_app.hEditLog, EM_REPLACESEL, FALSE, (LPARAM)w.c_str());
}

// ---- Owner-draw button (Razer style) -------------------------------------

namespace {

WNDPROC g_origButtonProc = nullptr;

LRESULT CALLBACK button_subclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return CallWindowProcW(g_origButtonProc, hwnd, msg, wp, lp);
}

void draw_owner_button(LPDRAWITEMSTRUCT dis, bool danger = false) {
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool focus   = (dis->itemState & ODS_FOCUS)    != 0;
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;

    COLORREF base = danger ? razer::kDanger : razer::kAccent;
    COLORREF hi   = danger ? RGB(0xFF, 0x6A, 0x6A) : razer::kAccentHi;
    COLORREF lo   = danger ? RGB(0xA8, 0x28, 0x28) : razer::kAccentLo;

    COLORREF border = pressed ? lo : (focus ? hi : base);
    COLORREF fill   = razer::kPanel;
    COLORREF text   = disabled ? razer::kTextDim : (pressed ? hi : razer::kText);

    if (disabled) { border = razer::kBorder; }

    RECT r = dis->rcItem;

    // background fill
    HBRUSH brFill = CreateSolidBrush(fill);
    FillRect(dis->hDC, &r, brFill);
    DeleteObject(brFill);

    // 1px border
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HPEN oldPen = (HPEN)SelectObject(dis->hDC, pen);
    HBRUSH oldBr = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
    Rectangle(dis->hDC, r.left, r.top, r.right, r.bottom);
    SelectObject(dis->hDC, oldPen);
    SelectObject(dis->hDC, oldBr);
    DeleteObject(pen);

    // accent left bar (Razer signature)
    if (!disabled) {
        RECT bar = r;
        bar.left += 1; bar.right = bar.left + 3;
        bar.top += 1;  bar.bottom -= 1;
        HBRUSH brBar = CreateSolidBrush(base);
        FillRect(dis->hDC, &bar, brBar);
        DeleteObject(brBar);
    }

    // text
    wchar_t buf[256];
    GetWindowTextW(dis->hwndItem, buf, 256);
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, text);
    HFONT old = (HFONT)SelectObject(dis->hDC, g_app.hFontBold);
    RECT textRect = r;
    textRect.left += 8;
    DrawTextW(dis->hDC, buf, -1, &textRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(dis->hDC, old);
}

} // namespace

// ---- Profile rendering ----------------------------------------------------

static std::string render_profile(const vhid::DeviceProfile& p) {
    std::ostringstream oss;
    oss << "DEVICE IDENTITY\r\n"
        << "  Vendor ID      : 0x" << std::hex << std::setw(4) << std::setfill('0') << p.vendor_id << "\r\n"
        << "  Product ID     : 0x" << std::setw(4) << std::setfill('0') << p.product_id << "\r\n"
        << "  Version        : 0x" << std::setw(4) << std::setfill('0') << p.version_number << std::dec << "\r\n"
        << "  Manufacturer   : " << p.manufacturer << "\r\n"
        << "  Product        : " << p.product << "\r\n"
        << "  Serial Number  : " << p.serial_number << "\r\n"
        << "\r\nTOP-LEVEL COLLECTION\r\n"
        << "  Usage Page     : 0x" << std::hex << p.usage_page << "\r\n"
        << "  Usage          : 0x" << p.usage << std::dec << "\r\n"
        << "\r\nREPORTS\r\n"
        << "  Input          : " << p.input_reports.size() << " report(s)\r\n"
        << "  Output         : " << p.output_reports.size() << " report(s)\r\n"
        << "  Feature        : " << p.feature_reports.size() << " report(s)\r\n"
        << "\r\nREPORT DESCRIPTOR (" << p.report_descriptor.size() << " bytes)\r\n";

    for (std::size_t i = 0; i < p.report_descriptor.size(); ++i) {
        if (i % 16 == 0) oss << "  ";
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)p.report_descriptor[i] << ' ';
        if (i % 16 == 15) oss << "\r\n";
    }
    if (p.report_descriptor.size() % 16 != 0) oss << "\r\n";
    return oss.str();
}

static void show_profile(const vhid::DeviceProfile& p) {
    g_app.currentProfile = p;
    auto txt = render_profile(p);
    SetWindowTextW(g_app.hEditDetails, utf8_to_wide(txt).c_str());
}

// ---- Device list ----------------------------------------------------------

static void refresh_devices() {
    g_app.devices = vhid::enumerate_hid_devices();
    ListView_DeleteAllItems(g_app.hListDevices);

    for (std::size_t i = 0; i < g_app.devices.size(); ++i) {
        const auto& d = g_app.devices[i];
        std::wstring product = utf8_to_wide(d.product.empty() ? "<unnamed>" : d.product);
        wchar_t vid[16], pid[16];
        wsprintfW(vid, L"0x%04X", d.vendor_id);
        wsprintfW(pid, L"0x%04X", d.product_id);

        LVITEMW lv{};
        lv.mask = LVIF_TEXT;
        lv.iItem = (int)i;
        lv.pszText = product.data();
        ListView_InsertItem(g_app.hListDevices, &lv);
        ListView_SetItemText(g_app.hListDevices, (int)i, 1, vid);
        ListView_SetItemText(g_app.hListDevices, (int)i, 2, pid);
    }

    wchar_t buf[64];
    wsprintfW(buf, L"  %d HID device(s) found", (int)g_app.devices.size());
    set_status(buf);
}

// ---- Capture & save -------------------------------------------------------

static int selected_device_index() {
    int sel = ListView_GetNextItem(g_app.hListDevices, -1, LVNI_SELECTED);
    return sel;
}

static void capture_selected() {
    int idx = selected_device_index();
    if (idx < 0 || (std::size_t)idx >= g_app.devices.size()) {
        MessageBoxW(g_app.hMain, L"Select a device in the list first.",
                    L"VHID", MB_OK | MB_ICONINFORMATION);
        return;
    }
    const auto& d = g_app.devices[idx];
    log_line("Inspecting: " + (d.product.empty() ? d.device_path : d.product));
    auto profile = vhid::inspect_device(d.device_path);
    if (!profile) {
        log_line("  -> inspect_device FAILED (device may be exclusively held)");
        MessageBoxW(g_app.hMain, L"Failed to capture this device. It may be exclusively held by the system.",
                    L"VHID", MB_OK | MB_ICONERROR);
        return;
    }
    show_profile(*profile);
    log_line("  -> captured " + std::to_string(profile->report_descriptor.size())
             + " descriptor bytes, "
             + std::to_string(profile->input_reports.size())   + " input / "
             + std::to_string(profile->output_reports.size())  + " output / "
             + std::to_string(profile->feature_reports.size()) + " feature reports");
}

static void save_profile() {
    if (!g_app.currentProfile) {
        MessageBoxW(g_app.hMain, L"Nothing to save: capture or load a profile first.",
                    L"VHID", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t path[MAX_PATH] = L"vhid-profile.json";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_app.hMain;
    ofn.lpstrFilter = L"VHID profile (*.json)\0*.json\0All files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"json";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return;

    if (g_app.currentProfile->save(wide_to_utf8(path))) {
        log_line("Saved profile to " + wide_to_utf8(path));
    } else {
        MessageBoxW(g_app.hMain, L"Failed to write file.", L"VHID", MB_OK | MB_ICONERROR);
    }
}

static void load_profile() {
    wchar_t path[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_app.hMain;
    ofn.lpstrFilter = L"VHID profile (*.json)\0*.json\0All files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;

    auto loaded = vhid::DeviceProfile::load(wide_to_utf8(path));
    if (!loaded) {
        MessageBoxW(g_app.hMain, L"Failed to parse JSON.", L"VHID", MB_OK | MB_ICONERROR);
        return;
    }
    show_profile(*loaded);
    log_line("Loaded profile from " + wide_to_utf8(path));
}

// ---- Watch thread ---------------------------------------------------------

static void watch_thread_proc() {
    std::set<std::string> baseline;
    for (const auto& d : vhid::enumerate_hid_devices()) baseline.insert(d.device_path);

    log_line("Watching for new HID device... plug one in.");
    while (g_app.watchActive.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        auto cur = vhid::enumerate_hid_devices();
        for (auto& d : cur) {
            if (!baseline.count(d.device_path)) {
                baseline.insert(d.device_path);
                auto* heap = new vhid::HidDeviceInfo(d);
                PostMessage(g_app.hMain, WM_APP_DEVICE_FOUND, 0, (LPARAM)heap);
                g_app.watchActive.store(false);
                return;
            }
        }
    }
    log_line("Watch stopped.");
}

static void start_watch() {
    if (g_app.watchActive.load()) return;
    g_app.watchActive.store(true);
    if (g_app.watchThread.joinable()) g_app.watchThread.join();
    g_app.watchThread = std::thread(watch_thread_proc);
}

// ---- Mock device control --------------------------------------------------

static void mock_start() {
    if (g_app.mockRunning.load()) {
        log_line("Mock device already running.");
        return;
    }
    try {
        auto t = std::make_unique<vhid::NamedPipeMockTransport>(
            vhid::NamedPipeMockTransport::Role::Device);
        g_app.mockDevice = std::make_unique<vhid::Device>(std::move(t));

        g_app.mockDevice->on_output([](const vhid::OutputReport& r) {
            log_line("[OUTPUT] " + bytes_to_hex(r.wire.data(), r.wire.size()));
        });
        g_app.mockDevice->on_feature_get([] {
            log_line("[FEAT-GET] (returning empty)");
            return vhid::FeatureReport{};
        });
        g_app.mockDevice->on_feature_set([](const vhid::FeatureReport& r) {
            log_line("[FEAT-SET] " + bytes_to_hex(r.wire.data(), r.wire.size()));
        });
        g_app.mockDevice->open();
        g_app.mockRunning.store(true);
        log_line("Mock device started (named pipe). Run vhid-cli mock-host or consumer_app to connect.");
        if (g_app.currentProfile) {
            log_line("  emulating profile: VID=0x"
                     + bytes_to_hex(reinterpret_cast<std::uint8_t*>(&g_app.currentProfile->vendor_id), 2)
                     + " PID=0x"
                     + bytes_to_hex(reinterpret_cast<std::uint8_t*>(&g_app.currentProfile->product_id), 2)
                     + " (" + g_app.currentProfile->product + ")");
        }
    } catch (const std::exception& e) {
        log_line(std::string("mock_start failed: ") + e.what());
        g_app.mockDevice.reset();
    }
}

static void mock_stop() {
    if (!g_app.mockRunning.load()) return;
    try { g_app.mockDevice->close(); } catch (...) {}
    g_app.mockDevice.reset();
    g_app.mockRunning.store(false);
    log_line("Mock device stopped.");
}

static void inject_input() {
    if (!g_app.mockRunning.load() || !g_app.mockDevice) {
        MessageBoxW(g_app.hMain, L"Start the mock device first.",
                    L"VHID", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t buf[1024];
    GetWindowTextW(g_app.hEditInputHex, buf, 1024);
    auto bytes = parse_hex(wide_to_utf8(buf));
    if (bytes.empty()) {
        MessageBoxW(g_app.hMain, L"Enter input bytes as hex (e.g. 11 22 33).",
                    L"VHID", MB_OK | MB_ICONINFORMATION);
        return;
    }
    try {
        vhid::InputReport r;
        r.set_payload(bytes.data(), bytes.size());
        g_app.mockDevice->submit_input(r);
        log_line("[INPUT-SENT] " + bytes_to_hex(bytes.data(), bytes.size()));
    } catch (const std::exception& e) {
        log_line(std::string("inject_input failed: ") + e.what());
    }
}

// ---- Window layout --------------------------------------------------------

static void create_controls(HWND hwnd) {
    HINSTANCE hi = g_app.hInst;

    auto mkBtn = [&](int id, const wchar_t* text, int x, int y, int w, int h, HWND parent) {
        return CreateWindowExW(0, L"BUTTON", text,
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            x, y, w, h, parent, (HMENU)(INT_PTR)id, hi, nullptr);
    };
    auto mkStatic = [&](const wchar_t* text, int x, int y, int w, int h, HWND parent) {
        return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
            x, y, w, h, parent, nullptr, hi, nullptr);
    };

    // ---- Top status bar ----
    g_app.hStatus = mkStatic(L"  ready", 0, 0, 1100, 28, hwnd);

    // ---- Left panel: device list ----
    mkStatic(L"DEVICES", 16, 40, 200, 18, hwnd);
    mkBtn(IDC_BTN_REFRESH, L"REFRESH",   16, 60, 110, 30, hwnd);
    mkBtn(IDC_BTN_WATCH,   L"WATCH NEW", 134, 60, 110, 30, hwnd);

    g_app.hListDevices = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        16, 100, 360, 380, hwnd, (HMENU)(INT_PTR)IDC_LIST_DEVICES, hi, nullptr);
    ListView_SetExtendedListViewStyle(g_app.hListDevices,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = 200; col.pszText = (LPWSTR)L"Product";    ListView_InsertColumn(g_app.hListDevices, 0, &col);
    col.cx = 70;  col.pszText = (LPWSTR)L"VID";        ListView_InsertColumn(g_app.hListDevices, 1, &col);
    col.cx = 70;  col.pszText = (LPWSTR)L"PID";        ListView_InsertColumn(g_app.hListDevices, 2, &col);

    mkBtn(IDC_BTN_CAPTURE, L"CAPTURE SELECTED", 16,  490, 175, 32, hwnd);
    mkBtn(IDC_BTN_LOAD,    L"LOAD PROFILE",     200, 490, 176, 32, hwnd);

    // ---- Middle panel: profile details ----
    mkStatic(L"PROFILE", 396, 40, 200, 18, hwnd);
    mkBtn(IDC_BTN_SAVE, L"SAVE TO JSON", 396, 60, 130, 30, hwnd);

    g_app.hEditDetails = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        396, 100, 360, 422, hwnd, (HMENU)(INT_PTR)IDC_EDIT_DETAILS, hi, nullptr);

    // ---- Right panel: mock device ----
    mkStatic(L"MOCK DEVICE", 776, 40, 200, 18, hwnd);
    mkBtn(IDC_BTN_MOCK_START, L"START", 776, 60, 100, 30, hwnd);
    mkBtn(IDC_BTN_MOCK_STOP,  L"STOP",  884, 60, 100, 30, hwnd);

    mkStatic(L"Input report (hex):", 776, 100, 200, 18, hwnd);
    g_app.hEditInputHex = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"11 22 33 44",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        776, 120, 280, 24, hwnd, (HMENU)(INT_PTR)IDC_EDIT_INPUT_HEX, hi, nullptr);
    mkBtn(IDC_BTN_INJECT_INPUT, L"SUBMIT INPUT", 776, 150, 280, 30, hwnd);

    mkStatic(L"LOG", 776, 200, 200, 18, hwnd);
    mkBtn(IDC_BTN_CLEAR_LOG, L"CLEAR", 1000, 198, 60, 22, hwnd);
    g_app.hEditLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        776, 224, 284, 298, hwnd, (HMENU)(INT_PTR)IDC_EDIT_LOG, hi, nullptr);

    // Apply fonts
    auto setFont = [&](HWND h, HFONT f) {
        if (h) SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE);
    };
    EnumChildWindows(hwnd, [](HWND child, LPARAM) -> BOOL {
        wchar_t cls[64];
        GetClassNameW(child, cls, 64);
        SendMessageW(child, WM_SETFONT, (WPARAM)g_app.hFont, TRUE);
        return TRUE;
    }, 0);
    setFont(g_app.hEditDetails, g_app.hFontMono);
    setFont(g_app.hEditLog,     g_app.hFontMono);
    setFont(g_app.hEditInputHex,g_app.hFontMono);
    setFont(g_app.hStatus,      g_app.hFontBold);
}

static void layout_controls(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    if (W < 700 || H < 400) return;

    SetWindowPos(g_app.hStatus, nullptr, 0, 0, W, 28, SWP_NOZORDER);

    // Left column: 360 wide
    int leftX = 16, leftW = 360;
    int midX  = leftX + leftW + 20, midW = 360;
    int rightX = midX + midW + 20;
    int rightW = W - rightX - 16;
    if (rightW < 280) rightW = 280;

    int topY = 40, hdrH = 18;
    int btnRowY = topY + hdrH + 4;        // 60
    int contentY = btnRowY + 40;          // 100
    int bottomBtnY = H - 40;              // capture/load row

    SetWindowPos(g_app.hListDevices, nullptr, leftX, contentY, leftW, bottomBtnY - contentY - 8, SWP_NOZORDER);
    HWND hCap = GetDlgItem(hwnd, IDC_BTN_CAPTURE);
    HWND hLoad = GetDlgItem(hwnd, IDC_BTN_LOAD);
    SetWindowPos(hCap,  nullptr, leftX,        bottomBtnY, (leftW-8)/2, 32, SWP_NOZORDER);
    SetWindowPos(hLoad, nullptr, leftX + (leftW-8)/2 + 8, bottomBtnY, (leftW-8)/2, 32, SWP_NOZORDER);

    SetWindowPos(g_app.hEditDetails, nullptr, midX, contentY, midW, H - contentY - 16, SWP_NOZORDER);

    // Right column
    HWND hClear = GetDlgItem(hwnd, IDC_BTN_CLEAR_LOG);
    SetWindowPos(g_app.hEditInputHex, nullptr, rightX, 120, rightW, 24, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, IDC_BTN_INJECT_INPUT), nullptr, rightX, 150, rightW, 30, SWP_NOZORDER);
    SetWindowPos(hClear, nullptr, rightX + rightW - 60, 198, 60, 22, SWP_NOZORDER);
    SetWindowPos(g_app.hEditLog, nullptr, rightX, 224, rightW, H - 224 - 16, SWP_NOZORDER);
}

// ---- Window proc ----------------------------------------------------------

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        g_app.hMain = hwnd;
        create_controls(hwnd);
        layout_controls(hwnd);
        refresh_devices();
        return 0;

    case WM_SIZE:
        layout_controls(hwnd);
        return 0;

    case WM_GETMINMAXINFO: {
        auto* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize.x = 1000;
        mmi->ptMinTrackSize.y = 600;
        return 0;
    }

    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, g_app.brBackground);
        return 1;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, razer::kAccent);
        SetBkColor(hdc, razer::kBackground);
        return (LRESULT)g_app.brBackground;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, razer::kText);
        SetBkColor(hdc, razer::kPanel);
        return (LRESULT)g_app.brPanel;
    }

    case WM_DRAWITEM: {
        auto* dis = (LPDRAWITEMSTRUCT)lp;
        if (dis->CtlType == ODT_BUTTON) {
            bool danger = (dis->CtlID == IDC_BTN_MOCK_STOP || dis->CtlID == IDC_BTN_CLEAR_LOG);
            draw_owner_button(dis, danger);
            return TRUE;
        }
        break;
    }

    case WM_NOTIFY: {
        auto* nm = (LPNMHDR)lp;
        if (nm->idFrom == IDC_LIST_DEVICES && nm->code == NM_DBLCLK) {
            capture_selected();
            return 0;
        }
        break;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BTN_REFRESH:    refresh_devices(); return 0;
        case IDC_BTN_WATCH:      start_watch();     return 0;
        case IDC_BTN_CAPTURE:    capture_selected(); return 0;
        case IDC_BTN_SAVE:       save_profile();    return 0;
        case IDC_BTN_LOAD:       load_profile();    return 0;
        case IDC_BTN_MOCK_START: mock_start();      return 0;
        case IDC_BTN_MOCK_STOP:  mock_stop();       return 0;
        case IDC_BTN_INJECT_INPUT: inject_input();  return 0;
        case IDC_BTN_CLEAR_LOG:  SetWindowTextW(g_app.hEditLog, L""); return 0;
        }
        break;

    case WM_APP_LOG: {
        std::unique_ptr<std::string> s((std::string*)lp);
        append_log_ui(*s);
        return 0;
    }

    case WM_APP_DEVICE_FOUND: {
        std::unique_ptr<vhid::HidDeviceInfo> info((vhid::HidDeviceInfo*)lp);
        log_line("New device detected: "
                 + (info->product.empty() ? info->device_path : info->product));
        refresh_devices();
        // Auto-select the matching row.
        for (std::size_t i = 0; i < g_app.devices.size(); ++i) {
            if (g_app.devices[i].device_path == info->device_path) {
                ListView_SetItemState(g_app.hListDevices, (int)i,
                                      LVIS_SELECTED | LVIS_FOCUSED,
                                      LVIS_SELECTED | LVIS_FOCUSED);
                ListView_EnsureVisible(g_app.hListDevices, (int)i, FALSE);
                break;
            }
        }
        return 0;
    }

    case WM_DESTROY:
        g_app.watchActive.store(false);
        if (g_app.watchThread.joinable()) g_app.watchThread.join();
        if (g_app.mockRunning.load()) mock_stop();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---- Entry point ----------------------------------------------------------

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    g_app.hInst = hInstance;

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    g_app.brBackground = CreateSolidBrush(razer::kBackground);
    g_app.brPanel      = CreateSolidBrush(razer::kPanel);
    g_app.brPanelAlt   = CreateSolidBrush(razer::kPanelAlt);
    g_app.brBorder     = CreateSolidBrush(razer::kBorder);

    LOGFONTW lf{};
    lf.lfHeight = -13;
    lf.lfWeight = FW_NORMAL;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lstrcpyW(lf.lfFaceName, L"Segoe UI");
    g_app.hFont = CreateFontIndirectW(&lf);
    lf.lfWeight = FW_BOLD;
    g_app.hFontBold = CreateFontIndirectW(&lf);
    lf.lfWeight = FW_NORMAL;
    lf.lfHeight = -12;
    lstrcpyW(lf.lfFaceName, L"Consolas");
    g_app.hFontMono = CreateFontIndirectW(&lf);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = g_app.brBackground;
    wc.lpszClassName = L"VHID_Main";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, L"VHID_Main",
        L"VHID Control Center",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 720,
        nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}
