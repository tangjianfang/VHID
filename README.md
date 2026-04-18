# VHID — Virtual HID Toolkit for Windows

A two-layer virtual HID toolkit written in modern C++17 for creating, testing, and managing virtual HID devices on Windows.

## Overview

| Layer | Description |
|-------|-------------|
| **Mock Transport** | Pure user-mode C++ — in-process and named-pipe `IHidTransport` implementations for testing without any kernel driver |
| **KMDF Driver** | Root-enumerated software HID device powered by Microsoft VHF, visible to any Win32 HID API consumer (`hid.dll`, SetupDi, `HidD_*`, `HidP_*`) |

Both layers share the same **Report Descriptor** and report (de)serialization code in `src/core`, so you can develop and test entirely in user-mode, then switch to the real driver transparently.

## Features

- **Vendor-defined HID descriptor** (Usage Page 0xFF00) with Input / Output / Feature reports
- **In-process mock transport** — zero-copy, callback-based, ideal for unit tests
- **Named-pipe mock transport** — cross-process binary protocol for integration testing
- **SDK facade** (`Device`) — simple synchronous API that auto-selects mock or driver transport
- **Interactive CLI** — hex-based injector for manual testing (`mock-device`, `mock-host`, `driver` modes)
- **Sample consumer app** — real-world Win32 HID enumeration & I/O example
- **KMDF + VHF driver scaffold** — kernel-side virtual HID with IOCTL control interface

## Build

### Prerequisites

- **CMake** ≥ 3.20
- **Visual Studio 2022** (MSVC v143)
- **WDK 11** (only for the kernel driver)

### User-mode components (CMake)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

### Kernel driver (separate build)

```powershell
.\driver\vhid\build-driver.ps1
.\driver\vhid\sign-dev.ps1      # test-sign the .sys
```

### Build targets

| Target | Type | Description |
|--------|------|-------------|
| `vhid_core` | Static lib | Shared report descriptor + POD report types |
| `vhid_mock` | Static lib | InProcess + NamedPipe mock transports |
| `vhid_sdk` | Static lib | User-facing Device API (auto-selects transport) |
| `vhid_cli` | Executable | Interactive HID injector CLI |
| `vhid_tests` | Executable | Unit test suite |
| `consumer_app` | Executable | Sample Win32 HID consumer |

## Usage

### CLI injector

```powershell
# Start named-pipe device side
.\build\src\cli\Debug\vhid_cli.exe mock-device

# In another terminal — start host side
.\build\src\cli\Debug\vhid_cli.exe mock-host
```

Commands: `in <hex>` (submit input), `out <hex>` (send output), `getf` (get feature), `setf <hex>` (set feature), `sleep <ms>`, `quit`.

### Smoke test

```powershell
.\scripts\smoke-mock.ps1
```

### Driver installation (admin)

```powershell
bcdedit /set testsigning on   # once, then reboot
.\installer\install.ps1       # installs driver + test cert
.\installer\uninstall.ps1     # removes driver cleanly
```

## Project layout

```
src/core/        Report descriptor + report types (shared by all layers)
src/mock/        IHidTransport interface + InProcess / NamedPipe implementations
src/sdk/         User-facing Device facade + driver transport stub
src/cli/         Interactive CLI injector
tests/           Unit tests (lightweight custom framework)
samples/         Sample HID consumer app (Win32 API)
driver/vhid/     KMDF + VHF kernel driver (VS2022 + WDK 11)
installer/       Install / uninstall / packaging scripts
scripts/         E2E smoke test
```

## Architecture

```
┌──────────────┐      ┌──────────────┐
│  Consumer App │      │   CLI Tool   │
│ (Win32 HID)  │      │  (injector)  │
└──────┬───────┘      └──────┬───────┘
       │                     │
       │              ┌──────▼───────┐
       │              │  SDK Device  │
       │              └──────┬───────┘
       │         ┌───────────┼───────────┐
       │         ▼           ▼           ▼
       │   InProcess    NamedPipe    Driver
       │   Transport    Transport    Transport
       │                                │
       │                         ┌──────▼───────┐
       └─────────────────────────► KMDF + VHF   │
                                 │  (kernel)    │
                                 └──────────────┘
```

## License

This project is currently under development. See individual files for details.
