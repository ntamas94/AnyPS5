#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

constexpr int REMOTEPLAY_CONNECTION_STATUS_DISCONNECT = 0;

extern "C" {

int APS5_VABI sceRemoteplayGetConnectionStatus(int user_id, int* status) {
 (void)user_id;
 if (!status) APS5_INVALID_ARG_EX;
 *status = REMOTEPLAY_CONNECTION_STATUS_DISCONNECT;
 return 0;
}

int APS5_VABI sceRemoteplayInitialize(void* heap, size_t heap_size) {
 if (!heap || heap_size == 0) APS5_INVALID_ARG_EX;
 return 0;
}

int APS5_VABI sceRemoteplayTerminate(void) {
 return 0;
}

int APS5_VABI sceRemoteplayApprove(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayClearAllRegistData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayClearConnectHistory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayConfirmDeviceRegist(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayDisconnect(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGeneratePinCode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetApMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetConnectHistory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetConnectUserId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetMbusDeviceInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetOperationStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetPrimaryUserId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetRemoteplayStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetRpMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeClose(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeFilterResult(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeGetEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeNotify(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeNotifyEventResult(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeSetCaret(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeSetText(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayIsRemoteOskReady(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayIsRemotePlaying(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayNotifyMbusDeviceRegistComplete(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayNotifyNpPushWakeup(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayNotifyPinCodeError(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayNotifySubmitCode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayNotifyUserDelete(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayPrintAllRegistData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayProhibit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayProhibitStreaming(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayServerLock(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayServerUnLock(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplaySetApMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplaySetLogLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplaySetProhibition(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplaySetProhibitionForVsh(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplaySetRpMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {
APS5_EXPORT("7QLrixwVHcU", Aps5LocalStub_libSceRemoteplay_0_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceRemoteplay_0_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceRemoteplayProhibitStreaming [libSceRemoteplay 7QLrixwVHcU]");
    return 0;
}
APS5_EXPORT("mrNh78tBpmg", Aps5LocalStub_libSceRemoteplay_1_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceRemoteplay_1_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceRemoteplayProhibit [libSceRemoteplay mrNh78tBpmg]");
    return 0;
}
APS5_EXPORT("xQeIryTX7dY", Aps5LocalStub_libSceRemoteplay_2_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceRemoteplay_2_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceRemoteplayApprove [libSceRemoteplay xQeIryTX7dY]");
    return 0;
}
}
