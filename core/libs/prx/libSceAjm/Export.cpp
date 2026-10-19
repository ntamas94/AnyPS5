#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAjmBatchJobControlBufferRa() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAjmBatchJobRunSplitBufferRa() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAjmBatchStartBuffer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {
APS5_EXPORT("ElslOCpOIns", Aps5LocalStub_libSceAjm_0_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceAjm_0_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceAjmBatchJobRunBufferRa [libSceAjm ElslOCpOIns]");
    return 0;
}
}
