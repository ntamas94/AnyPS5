#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

static constexpr int SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80552902);
static constexpr int SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND = static_cast<int>(0x80552904);
static constexpr int SCE_NP_WEBAPI_ERROR_PUSH_EVENT_FILTER_NOT_FOUND = static_cast<int>(0x8055290B);
static constexpr int SCE_NP_WEBAPI_ERROR_HANDLE_NOT_FOUND = static_cast<int>(0x8055290D);
static constexpr int SCE_NP_WEBAPI_ERROR_SERVICE_PUSH_EVENT_FILTER_NOT_FOUND = static_cast<int>(0x8055290E);
static constexpr int SCE_NP_WEBAPI_ERROR_SIGNED_IN_USER_NOT_FOUND = static_cast<int>(0x80552910);
static constexpr int SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_MAX = static_cast<int>(0x8055291A);

namespace {

struct LibraryContext {
    int libHttpCtxId;
    std::size_t poolSize;
    std::unordered_set<int> handles;
    std::unordered_set<int> pushEventFilters;
};

std::mutex contextsMutex;
std::unordered_map<int, LibraryContext> contexts;
int lastContext = 0;
int lastObject = 0;

int nextObject() {
    lastObject = lastObject % 0xefffffff + 1;
    return lastObject;
}

}

extern "C" {

int APS5_VABI sceNpWebApiAbortRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiCreateContext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiCreateHandle(int libCtxId) {
    std::lock_guard lock(contextsMutex);
    const auto context = contexts.find(libCtxId);
    if (context == contexts.end()) return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    const auto handle = nextObject();
    context->second.handles.insert(handle);
    return handle;
}

int APS5_VABI sceNpWebApiCreatePushEventFilter(int libCtxId, const void* filterParam, std::size_t filterParamNum) {
    if (!filterParam || filterParamNum == 0) return SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(contextsMutex);
    const auto context = contexts.find(libCtxId);
    if (context == contexts.end()) return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    const auto filter = nextObject();
    context->second.pushEventFilters.insert(filter);
    return filter;
}

int APS5_VABI sceNpWebApiCreateRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiCreateServicePushEventFilter(int libCtxId, int handleId, const char* npServiceName, std::uint32_t npServiceLabel, const void* filterParam, std::size_t filterParamNum) {
    (void)npServiceLabel;
    if (!npServiceName || !filterParam || filterParamNum == 0) return SCE_NP_WEBAPI_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(contextsMutex);
    const auto context = contexts.find(libCtxId);
    if (context == contexts.end()) return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    if (!context->second.handles.contains(handleId)) return SCE_NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
    return SCE_NP_WEBAPI_ERROR_SIGNED_IN_USER_NOT_FOUND;
}

int APS5_VABI sceNpWebApiDeleteContext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiDeleteHandle(int libCtxId, int handleId) {
    std::lock_guard lock(contextsMutex);
    const auto context = contexts.find(libCtxId);
    if (context == contexts.end()) return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    return context->second.handles.erase(handleId) != 0 ? 0 : SCE_NP_WEBAPI_ERROR_HANDLE_NOT_FOUND;
}

int APS5_VABI sceNpWebApiDeletePushEventFilter(int libCtxId, int filterId) {
    std::lock_guard lock(contextsMutex);
    const auto context = contexts.find(libCtxId);
    if (context == contexts.end()) return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    return context->second.pushEventFilters.erase(filterId) != 0 ? 0 : SCE_NP_WEBAPI_ERROR_PUSH_EVENT_FILTER_NOT_FOUND;
}

int APS5_VABI sceNpWebApiDeleteRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiDeleteServicePushEventFilter(int libCtxId, int filterId) {
    (void)filterId;
    std::lock_guard lock(contextsMutex);
    if (!contexts.contains(libCtxId)) return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
    return SCE_NP_WEBAPI_ERROR_SERVICE_PUSH_EVENT_FILTER_NOT_FOUND;
}

int APS5_VABI sceNpWebApiGetHttpResponseHeaderValue() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiGetHttpResponseHeaderValueLength() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiGetHttpStatusCode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiInitialize(int libHttpCtxId, std::size_t poolSize) {
    std::lock_guard lock(contextsMutex);
    for (int attempt = 0; attempt < 0x7fff; ++attempt) {
        lastContext = lastContext % 0x7fff + 1;
        if (contexts.contains(lastContext)) continue;
        contexts[lastContext] = {libHttpCtxId, poolSize, {}, {}};
        return lastContext;
    }
    return SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_MAX;
}

int APS5_VABI sceNpWebApiReadData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiRegisterPushEventCallback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiRegisterServicePushEventCallback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiSendRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiTerminate(int libCtxId) {
    std::lock_guard lock(contextsMutex);
    return contexts.erase(libCtxId) != 0 ? 0 : SCE_NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
}

int APS5_VABI sceNpWebApiUnregisterPushEventCallback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiUnregisterServicePushEventCallback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpWebApiUtilityParseNpId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {
APS5_EXPORT("3OnubUs02UM", Aps5LocalStub_libSceNpWebApi_0_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceNpWebApi_0_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceNpWebApiGetMemoryPoolStats [libSceNpWebApi 3OnubUs02UM]");
    return 0;
}
APS5_EXPORT("KjNeZ-29ysQ", Aps5LocalStub_libSceNpWebApi_1_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceNpWebApi_1_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceNpWebApiSendRequest2 [libSceNpWebApi KjNeZ-29ysQ]");
    return 0;
}
APS5_EXPORT("M2BUB+DNEGE", Aps5LocalStub_libSceNpWebApi_2_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceNpWebApi_2_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceNpWebApiCreateExtdPushEventFilter [libSceNpWebApi M2BUB+DNEGE]");
    return 0;
}
APS5_EXPORT("PqCY25FMzPs", Aps5LocalStub_libSceNpWebApi_3_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceNpWebApi_3_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceNpWebApiUnregisterExtdPushEventCallback [libSceNpWebApi PqCY25FMzPs]");
    return 0;
}
APS5_EXPORT("jhXKGQJ4egI", Aps5LocalStub_libSceNpWebApi_4_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceNpWebApi_4_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceNpWebApiRegisterExtdPushEventCallbackA [libSceNpWebApi jhXKGQJ4egI]");
    return 0;
}
APS5_EXPORT("pfaJtb7SQ80", Aps5LocalStub_libSceNpWebApi_5_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceNpWebApi_5_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceNpWebApiDeleteExtdPushEventFilter [libSceNpWebApi pfaJtb7SQ80]");
    return 0;
}
APS5_EXPORT("zk6c65xoyO0", Aps5LocalStub_libSceNpWebApi_6_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceNpWebApi_6_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceNpWebApiCreateContextA [libSceNpWebApi zk6c65xoyO0]");
    return 0;
}
}
