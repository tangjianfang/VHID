// Driver-backed transport: talks to the KMDF/VHF driver through the
// \\.\VHidControl symbolic link using IOCTLs declared in driver/vhid/Public.h.
//
// NOTE: This is a stub for Phase 4 to compile and run end-to-end with the mock
// transport in tests. Once the driver from Phase 3 is built and installed, the
// IOCTL plumbing here is the only thing that needs to be filled in; the SDK
// public API does not change.
#include "vhid/vhid_device.hpp"

#include <stdexcept>

namespace vhid {

std::unique_ptr<IHidTransport> open_driver_transport() {
    // TODO(Phase 3): open \\.\VHidControl via CreateFileW, exchange IOCTLs:
    //   IOCTL_VHID_SUBMIT_INPUT_REPORT  (user -> driver -> VhfReadReportSubmit)
    //   IOCTL_VHID_PEND_OUTPUT_REPORT   (inverted call: driver completes when
    //                                     a host writes an output report)
    //   IOCTL_VHID_PEND_FEATURE_REQUEST (inverted call for Get/SetFeature)
    //   IOCTL_VHID_COMPLETE_FEATURE_GET (user supplies the feature payload)
    throw std::runtime_error(
        "vhid driver transport not yet implemented; install the KMDF driver "
        "or use the mock transports for now.");
}

} // namespace vhid
