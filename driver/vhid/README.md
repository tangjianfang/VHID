# Driver: vhid (KMDF + VHF)

This folder is the kernel-mode side of the project. It is **not** part of the
CMake build because it requires the Windows Driver Kit (WDK) and a separate
MSBuild toolchain.

## Build

1. Install Visual Studio 2022 with the **Desktop C++** workload, then install
   the matching **Windows Driver Kit (WDK 11)** and the SDK extension.
2. Open Visual Studio and create a new **Kernel Mode Driver, Empty (KMDF)**
   project named `vhid` in this folder, then add:
   - `Driver.cpp`
   - `Vhf.cpp`
   - `Public.h`
   - `vhid.inf` (set as the driver INF)
3. Project properties:
   - **Linker → Input → Additional Dependencies**: add `Vhf.lib`.
   - **C/C++ → General → Additional Include Directories**: ensure
     `$(SDK_INC_PATH)` and the WDK include paths are present.
   - **Driver Settings → General → Target OS Version**: Windows 10 or later.
4. Build x64 / Debug. The output `vhid.sys` + `vhid.inf` + `vhid.cat` go in
   the build output folder.

## Install (test machine)

```powershell
# Once, as admin; reboot afterward:
bcdedit /set testsigning on

# Sign the freshly built driver (creates a self-signed cert on first run):
.\driver\vhid\sign-dev.ps1

# Each install:
devcon install vhid.inf Root\VHID
# or:
pnputil /add-driver vhid.inf /install
```

For release builds use a real EV certificate and submit the package to
Microsoft Partner Center for attestation signing. `sign-dev.ps1` is
**developer-machines only**.

After install you should see *Virtual HID Device (VHID)* in Device Manager
under *Human Interface Devices*, and `\\.\VHidControl` will exist for the SDK.

## Status

This is the Phase 3 scaffold. Working today:
- Driver entry, KMDF device add, control device + symlink.
- VHF descriptor registration + start.
- `IOCTL_VHID_SUBMIT_INPUT_REPORT` → `VhfReadReportSubmit`.
- Output reports forwarded to user-mode through inverted calls.

TODO before parity with the Mock layer:
- Inverted-call completion for `Get_Feature` (currently returns zeros).
- Multi-instance / `--instance` support.
- Codegen of `g_ReportDescriptor` from `src/core` to remove duplication.
