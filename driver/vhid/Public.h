// Public IOCTL contract between vhid-sdk (user-mode) and the KMDF/VHF driver.
// Included by both sides; safe to compile in user-mode and kernel-mode.
#pragma once

#include <initguid.h>

// {7B9F2B61-1F3A-4E91-9F35-8E0F4B8E7AD1}
DEFINE_GUID(GUID_DEVINTERFACE_VHID_CONTROL,
    0x7b9f2b61, 0x1f3a, 0x4e91, 0x9f, 0x35, 0x8e, 0x0f, 0x4b, 0x8e, 0x7a, 0xd1);

#define VHID_CONTROL_DEVICE_NAME_W      L"\\Device\\VHidControl"
#define VHID_CONTROL_SYMLINK_NAME_W     L"\\DosDevices\\VHidControl"
#define VHID_CONTROL_USERMODE_PATH_W    L"\\\\.\\VHidControl"

// IOCTL function codes. We use METHOD_BUFFERED for simplicity.
#ifndef CTL_CODE
#include <winioctl.h>
#endif

#define IOCTL_VHID_SUBMIT_INPUT_REPORT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_WRITE_DATA)

// Inverted call: user-mode pends this; driver completes it when the host
// writes an Output report. Output buffer receives the wire bytes.
#define IOCTL_VHID_PEND_OUTPUT_REPORT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_DATA)

// Inverted call: user-mode pends this; driver completes it on a Get/Set
// Feature request. Output buffer receives:
//   [u8 op]  0 = Get (user must follow up with COMPLETE_FEATURE_GET),
//            1 = Set (followed by feature wire bytes)
//   [feature wire bytes]  (only for Set)
#define IOCTL_VHID_PEND_FEATURE_REQUEST \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_READ_DATA)

// Companion to PEND_FEATURE_REQUEST(Get): user-mode supplies the feature
// payload that the driver returns to the host.
#define IOCTL_VHID_COMPLETE_FEATURE_GET \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_WRITE_DATA)
