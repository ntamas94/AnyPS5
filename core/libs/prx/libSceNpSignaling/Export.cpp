#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpSignalingActivateConnection() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSignalingCreateContext(const void*, void*, void*, std::uint32_t* context) {
    if (!context) return -1;
    *context = 1;
    return 0;
}

int APS5_VABI sceNpSignalingDeleteContext(std::uint32_t) {
    return 0;
}

int APS5_VABI sceNpSignalingGetConnectionInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSignalingInitialize(std::int64_t, std::int32_t, std::int32_t, std::int64_t) {
    return 0;
}

int APS5_VABI sceNpSignalingSetContextOption(std::uint32_t, std::int32_t, std::int32_t) {
    return 0;
}

int APS5_VABI sceNpSignalingTerminate() {
    return 0;
}

int APS5_VABI sceNpSignalingTerminateConnection() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {
APS5_EXPORT("U8AQMlOFBc8", Aps5LocalStub_libSceNpSignaling_0_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceNpSignaling_0_nid_no_patch(std::uint32_t, void* info) {
    if (!info) return -1;
    auto* words = static_cast<std::uint32_t*>(info);
    words[2] = words[3] = words[4] = 0;
    return 0;
}
APS5_EXPORT("ZPLavCKqAB0", Aps5LocalStub_libSceNpSignaling_1_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceNpSignaling_1_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceNpSignalingActivateConnectionA [libSceNpSignaling ZPLavCKqAB0]");
    return 0;
}
APS5_EXPORT("bD-JizUb3JM", Aps5LocalStub_libSceNpSignaling_2_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceNpSignaling_2_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceNpSignalingGetConnectionStatus [libSceNpSignaling bD-JizUb3JM]");
    return 0;
}
APS5_EXPORT("dDLNFdY8dws", Aps5LocalStub_libSceNpSignaling_3_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceNpSignaling_3_nid_no_patch(std::int32_t, void*, void*, std::uint32_t* context) {
    if (!context) return -1;
    *context = 1;
    return 0;
}
}
