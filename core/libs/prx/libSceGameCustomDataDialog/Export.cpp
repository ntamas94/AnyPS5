#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceGameCustomDataDialogGetResult() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameCustomDataDialogInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameCustomDataDialogOpen() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameCustomDataDialogTerminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameCustomDataDialogUpdateStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {
APS5_EXPORT("QHFJB2F0p3k", Aps5LocalStub_libSceGameCustomDataDialog_0_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGameCustomDataDialog_0_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGameCustomDataDialogOpenA [libSceGameCustomDataDialog QHFJB2F0p3k]");
    return 0;
}
APS5_EXPORT("TWi-wZVp5Pc", Aps5LocalStub_libSceGameCustomDataDialog_1_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGameCustomDataDialog_1_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGameCustomDataDialogGetResultA [libSceGameCustomDataDialog TWi-wZVp5Pc]");
    return 0;
}
}
