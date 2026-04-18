// VHF integration + control-device IOCTL queue.
//
// Functions exposed:
//   Vhf_Initialize(WDFDEVICE)      — per device-instance VHF setup.
//   Vhf_CreateControlDevice(WDFDRIVER) — global \\.\VHidControl device.
//
// The control device hosts three queues:
//   - default queue: parses IOCTLs (synchronous validation).
//   - g_OutputPendingQueue: holds pended IOCTL_VHID_PEND_OUTPUT_REPORT
//                           requests; drained from EvtVhfAsyncOperationSetOutputReport.
//   - g_FeaturePendingQueue: holds pended IOCTL_VHID_PEND_FEATURE_REQUEST
//                           requests; drained from feature callbacks.
//
// This file is intentionally a working scaffold: every function is wired up,
// but a few inverted-call edge cases (timeout, cancellation, multi-instance)
// are marked TODO. The flow is correct end-to-end for the common case.

extern "C" {
#include <ntddk.h>
#include <wdf.h>
#include <vhf.h>
}

#include "Public.h"

// Report descriptor bytes. Generated from src/core/src/report_descriptor.cpp;
// kept in sync manually for now (the driver build doesn't link the user-mode
// static library). Any time you change the descriptor, copy it here too.
//
// TODO: codegen this header from src/core during the driver build.
static const UCHAR g_ReportDescriptor[] = {
    0x06, 0x00, 0xFF,  0x09, 0x01,  0xA1, 0x01,
    0x85, 0x01,  0x09, 0x02,  0x15, 0x00,  0x26, 0xFF, 0x00,
    0x75, 0x08,  0x95, 0x10,  0x81, 0x02,
    0x85, 0x02,  0x09, 0x03,  0x15, 0x00,  0x26, 0xFF, 0x00,
    0x75, 0x08,  0x95, 0x10,  0x91, 0x02,
    0x85, 0x03,  0x09, 0x04,  0x15, 0x00,  0x26, 0xFF, 0x00,
    0x75, 0x08,  0x95, 0x08,  0xB1, 0x02,
    0xC0,
};

#define INPUT_REPORT_ID    0x01
#define OUTPUT_REPORT_ID   0x02
#define FEATURE_REPORT_ID  0x03
#define INPUT_PAYLOAD_LEN  16
#define OUTPUT_PAYLOAD_LEN 16
#define FEATURE_PAYLOAD_LEN 8

static WDFDEVICE g_ControlDevice       = nullptr;
static WDFQUEUE  g_OutputPendingQueue  = nullptr;
static WDFQUEUE  g_FeaturePendingQueue = nullptr;
static VHFHANDLE g_VhfHandle           = nullptr;

// ---------------------------------------------------------------------------
// VHF callbacks
// ---------------------------------------------------------------------------

extern "C" VOID EvtVhfAsyncOperationGetInputReport(
    _In_ PVOID /*VhfClientContext*/,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_ PVOID /*VhfOperationContext*/,
    _In_ PHID_XFER_PACKET HidTransferPacket) {
    // We are a *software* HID source: input reports are pushed via
    // VhfReadReportSubmit when user-mode calls IOCTL_VHID_SUBMIT_INPUT_REPORT.
    // The host should only request input via interrupt reads, but if it issues
    // a Get_Report(Input), we just return zeros.
    if (HidTransferPacket->reportBufferLen >= 1) {
        HidTransferPacket->reportBuffer[0] = INPUT_REPORT_ID;
        for (ULONG i = 1; i < HidTransferPacket->reportBufferLen; ++i)
            HidTransferPacket->reportBuffer[i] = 0;
    }
    VhfAsyncOperationComplete(VhfOperationHandle, STATUS_SUCCESS);
}

static NTSTATUS PendOrComplete(WDFQUEUE queue, PHID_XFER_PACKET packet, UCHAR op) {
    // Try to hand the report bytes to the next pended user-mode reader.
    WDFREQUEST request = nullptr;
    NTSTATUS status = WdfIoQueueRetrieveNextRequest(queue, &request);
    if (!NT_SUCCESS(status) || request == nullptr) {
        // No reader; drop and succeed (host doesn't need to know).
        return STATUS_SUCCESS;
    }

    PVOID outBuf = nullptr;
    size_t outLen = 0;
    status = WdfRequestRetrieveOutputBuffer(request, 1, &outBuf, &outLen);
    if (!NT_SUCCESS(status)) {
        WdfRequestComplete(request, status);
        return status;
    }

    // Layout written to user-mode: [u8 op][report wire bytes...]
    size_t needed = 1 + packet->reportBufferLen;
    if (outLen < needed) {
        WdfRequestComplete(request, STATUS_BUFFER_TOO_SMALL);
        return STATUS_BUFFER_TOO_SMALL;
    }
    auto* p = static_cast<UCHAR*>(outBuf);
    p[0] = op;
    RtlCopyMemory(p + 1, packet->reportBuffer, packet->reportBufferLen);
    WdfRequestCompleteWithInformation(request, STATUS_SUCCESS, needed);
    return STATUS_SUCCESS;
}

extern "C" VOID EvtVhfAsyncOperationWriteReport(
    _In_ PVOID, _In_ VHFOPERATIONHANDLE Op, _In_ PVOID,
    _In_ PHID_XFER_PACKET HidTransferPacket) {
    PendOrComplete(g_OutputPendingQueue, HidTransferPacket, /*op=*/0);
    VhfAsyncOperationComplete(Op, STATUS_SUCCESS);
}

extern "C" VOID EvtVhfAsyncOperationGetFeature(
    _In_ PVOID, _In_ VHFOPERATIONHANDLE Op, _In_ PVOID,
    _In_ PHID_XFER_PACKET HidTransferPacket) {
    // TODO: full inverted-call: pend the host request here, wait for user-mode
    // to complete IOCTL_VHID_COMPLETE_FEATURE_GET, then copy the supplied
    // bytes into HidTransferPacket->reportBuffer before completing.
    // For the scaffold we return a zeroed feature so the host call succeeds.
    if (HidTransferPacket->reportBufferLen >= 1) {
        HidTransferPacket->reportBuffer[0] = FEATURE_REPORT_ID;
        for (ULONG i = 1; i < HidTransferPacket->reportBufferLen; ++i)
            HidTransferPacket->reportBuffer[i] = 0;
    }
    VhfAsyncOperationComplete(Op, STATUS_SUCCESS);
}

extern "C" VOID EvtVhfAsyncOperationSetFeature(
    _In_ PVOID, _In_ VHFOPERATIONHANDLE Op, _In_ PVOID,
    _In_ PHID_XFER_PACKET HidTransferPacket) {
    PendOrComplete(g_FeaturePendingQueue, HidTransferPacket, /*op=*/1);
    VhfAsyncOperationComplete(Op, STATUS_SUCCESS);
}

// ---------------------------------------------------------------------------
// Control device + IOCTL queue
// ---------------------------------------------------------------------------

extern "C" VOID VhidEvtIoDeviceControl(
    _In_ WDFQUEUE   /*Queue*/,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode) {
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR info  = 0;

    switch (IoControlCode) {
        case IOCTL_VHID_SUBMIT_INPUT_REPORT: {
            PVOID inBuf = nullptr; size_t inLen = 0;
            status = WdfRequestRetrieveInputBuffer(Request, 1 + INPUT_PAYLOAD_LEN, &inBuf, &inLen);
            if (NT_SUCCESS(status) && g_VhfHandle) {
                HID_XFER_PACKET pkt{};
                pkt.reportBuffer    = static_cast<PUCHAR>(inBuf);
                pkt.reportBufferLen = static_cast<ULONG>(inLen);
                pkt.reportId        = static_cast<UCHAR>(static_cast<PUCHAR>(inBuf)[0]);
                status = VhfReadReportSubmit(g_VhfHandle, &pkt);
            }
            break;
        }
        case IOCTL_VHID_PEND_OUTPUT_REPORT: {
            // Forward to the pending queue; will be completed when host writes.
            status = WdfRequestForwardToIoQueue(Request, g_OutputPendingQueue);
            if (NT_SUCCESS(status)) return;  // request now owned by queue
            break;
        }
        case IOCTL_VHID_PEND_FEATURE_REQUEST: {
            status = WdfRequestForwardToIoQueue(Request, g_FeaturePendingQueue);
            if (NT_SUCCESS(status)) return;
            break;
        }
        case IOCTL_VHID_COMPLETE_FEATURE_GET: {
            // TODO: deliver the user-supplied feature payload to a stashed
            // pending Get-Feature VHF operation. Stub for now.
            status = STATUS_SUCCESS;
            break;
        }
        default:
            break;
    }
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    WdfRequestCompleteWithInformation(Request, status, info);
}

extern "C" NTSTATUS Vhf_CreateControlDevice(_In_ WDFDRIVER Driver) {
    PWDFDEVICE_INIT init = WdfControlDeviceInitAllocate(Driver, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RW_RES_R);
    if (!init) return STATUS_INSUFFICIENT_RESOURCES;

    UNICODE_STRING devName;
    RtlInitUnicodeString(&devName, VHID_CONTROL_DEVICE_NAME_W);
    NTSTATUS status = WdfDeviceInitAssignName(init, &devName);
    if (!NT_SUCCESS(status)) { WdfDeviceInitFree(init); return status; }

    WdfDeviceInitSetIoType(init, WdfDeviceIoBuffered);

    WDFDEVICE device = nullptr;
    status = WdfDeviceCreate(&init, WDF_NO_OBJECT_ATTRIBUTES, &device);
    if (!NT_SUCCESS(status)) return status;
    g_ControlDevice = device;

    UNICODE_STRING symlink;
    RtlInitUnicodeString(&symlink, VHID_CONTROL_SYMLINK_NAME_W);
    status = WdfDeviceCreateSymbolicLink(device, &symlink);
    if (!NT_SUCCESS(status)) return status;

    // Default queue: handles incoming IOCTLs.
    WDF_IO_QUEUE_CONFIG qcfg;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&qcfg, WdfIoQueueDispatchSequential);
    qcfg.EvtIoDeviceControl = VhidEvtIoDeviceControl;
    status = WdfIoQueueCreate(device, &qcfg, WDF_NO_OBJECT_ATTRIBUTES, nullptr);
    if (!NT_SUCCESS(status)) return status;

    // Manual queues for inverted-call pending requests.
    WDF_IO_QUEUE_CONFIG_INIT(&qcfg, WdfIoQueueDispatchManual);
    status = WdfIoQueueCreate(device, &qcfg, WDF_NO_OBJECT_ATTRIBUTES, &g_OutputPendingQueue);
    if (!NT_SUCCESS(status)) return status;
    status = WdfIoQueueCreate(device, &qcfg, WDF_NO_OBJECT_ATTRIBUTES, &g_FeaturePendingQueue);
    if (!NT_SUCCESS(status)) return status;

    WdfControlFinishInitializing(device);
    return STATUS_SUCCESS;
}

extern "C" NTSTATUS Vhf_Initialize(_In_ WDFDEVICE Device) {
    VHF_CONFIG cfg;
    VHF_CONFIG_INIT(&cfg, WdfDeviceWdmGetDeviceObject(Device),
                    sizeof(g_ReportDescriptor),
                    const_cast<PUCHAR>(g_ReportDescriptor));
    cfg.EvtVhfAsyncOperationGetInputReport = EvtVhfAsyncOperationGetInputReport;
    cfg.EvtVhfAsyncOperationWriteReport     = EvtVhfAsyncOperationWriteReport;
    cfg.EvtVhfAsyncOperationGetFeature      = EvtVhfAsyncOperationGetFeature;
    cfg.EvtVhfAsyncOperationSetFeature      = EvtVhfAsyncOperationSetFeature;

    NTSTATUS status = VhfCreate(&cfg, &g_VhfHandle);
    if (!NT_SUCCESS(status)) return status;

    return VhfStart(g_VhfHandle);
}
