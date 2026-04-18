// KMDF driver entry + Device add for vhid (Virtual HID using VHF).
//
// Build with VS2022 + WDK 11 (Windows Driver Kit). This file is intentionally
// kept small — see Vhf.cpp for the VHF integration and IOCTL queue logic.
//
// Driver lifecycle:
//   1. DriverEntry registers EvtDriverDeviceAdd.
//   2. Per device, EvtDriverDeviceAdd creates a WDFDEVICE, then calls
//      Vhf_Initialize() (in Vhf.cpp) which:
//         - Calls VhfCreate() with the report descriptor from src/core
//           (a copy is embedded in ReportDescriptor.c so the driver does
//            not depend on the user-mode build).
//         - Registers the four EvtVhfAsyncOperation* callbacks.
//         - Calls VhfStart().
//   3. A control device + symbolic link \\.\VHidControl is created so user-mode
//      tools (vhid-sdk) can submit Input reports and pend Output/Feature ones.
//
// Restrictions inherited from KMDF: no exceptions, no RTTI, no STL.

extern "C" {
#include <ntddk.h>
#include <wdf.h>
}

#include "Public.h"

// Forward declarations from Vhf.cpp
extern "C" NTSTATUS Vhf_Initialize(_In_ WDFDEVICE Device);
extern "C" NTSTATUS Vhf_CreateControlDevice(_In_ WDFDRIVER Driver);

extern "C" DRIVER_INITIALIZE        DriverEntry;
extern "C" EVT_WDF_DRIVER_DEVICE_ADD VhidEvtDeviceAdd;

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT  DriverObject,
                                _In_ PUNICODE_STRING RegistryPath) {
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, VhidEvtDeviceAdd);

    WDFDRIVER driver = nullptr;
    NTSTATUS status = WdfDriverCreate(DriverObject, RegistryPath,
                                      WDF_NO_OBJECT_ATTRIBUTES, &config, &driver);
    if (!NT_SUCCESS(status)) return status;

    // Create the user-mode control device once.
    status = Vhf_CreateControlDevice(driver);
    return status;
}

extern "C" NTSTATUS VhidEvtDeviceAdd(_In_    WDFDRIVER       /*Driver*/,
                                     _Inout_ PWDFDEVICE_INIT DeviceInit) {
    WDF_OBJECT_ATTRIBUTES attrs;
    WDF_OBJECT_ATTRIBUTES_INIT(&attrs);

    WDFDEVICE device = nullptr;
    NTSTATUS status = WdfDeviceCreate(&DeviceInit, &attrs, &device);
    if (!NT_SUCCESS(status)) return status;

    return Vhf_Initialize(device);
}
