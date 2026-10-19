#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <mutex>
#include <stdexcept>
#include <string>
#include <span>
#include <utility>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Eq/include/Event.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceGnmDriver/include/CommandStream.hpp"

namespace {

constexpr std::uint32_t OpNop = 0x10;
constexpr std::uint32_t OpClearState = 0x12;
constexpr std::uint32_t OpContextControl = 0x28;
constexpr std::uint32_t OpNumInstances = 0x2f;
constexpr std::uint32_t OpWriteData = 0x37;
constexpr std::uint32_t OpWaitRegMem = 0x3c;
constexpr std::uint32_t OpAcquireMem = 0x58;
constexpr std::uint32_t OpSetContextReg = 0x69;
constexpr std::uint32_t OpSetShReg = 0x76;
constexpr std::uint32_t OpSetUconfigReg = 0x79;
constexpr std::uint32_t OpSurfaceSync = 0x43;
constexpr std::uint32_t OpDmaData = 0x50;
constexpr std::uint32_t DmaRegisterDestination = 1u << 27;
constexpr std::uint32_t DmaNoDestinationIncrement = 1u << 29;
constexpr std::uint32_t CpDmaReadTagsBytes = 0xc08bu * 4;
constexpr std::uint32_t OpEventWriteEop = 0x47;
constexpr std::uint32_t OpEventWriteEos = 0x48;
constexpr std::uint32_t OpReleaseMem = 0x49;
constexpr std::uint32_t OpLoadConstRam = 0x80;
constexpr std::uint32_t OpWriteConstRam = 0x81;
constexpr std::uint32_t OpDumpConstRam = 0x83;
constexpr std::uint32_t OpIncrementCeCounter = 0x84;
constexpr std::uint32_t OpIncrementDeCounter = 0x85;
constexpr std::uint32_t OpWaitOnCeCounter = 0x86;
constexpr std::uint32_t OpWaitOnDeCounterDiff = 0x88;
constexpr std::uint32_t EventControlMask = 0xf3fu;
constexpr std::uint32_t EosStoreData = 2;
constexpr std::uint32_t ReleaseMemData32 = 1u << 29;
constexpr std::uint32_t GfxEndOfPipeEvent = 0x40;

constexpr std::uint32_t HwInitPacketWords = 0x100;
constexpr std::uint32_t FlipReserveWords = 64;
constexpr std::uint32_t PrepareFlipHeader = 0xc03e1000u;
constexpr std::uint32_t MarkerPush = 0x68750001u;
constexpr std::uint32_t MarkerPop = 0x68750002u;
constexpr std::uint32_t MarkerSet = 0x68750003u;
constexpr std::uint32_t MarkerColorPush = 0x6875000eu;
constexpr std::uint32_t PrepareFlip = 0x68750777u;
constexpr std::uint32_t PrepareFlipLabel = 0x68750778u;
constexpr std::uint32_t WriteDataToMemory = 0x500u;
constexpr std::uint32_t WaitMemoryEqual = 0x13u;
constexpr std::uint32_t MaxCommandBufferBytes = 0x3ffffc;
constexpr std::int16_t EvfiltGraphicsCore = -14;

constexpr int GNM_ERROR_SUBMISSION_FAILED_INVALID_ARGUMENT = static_cast<int>(0x80d11000u);
constexpr int GNM_ERROR_VALIDATION_NOT_ENABLED = static_cast<int>(0x80d13fffu);
constexpr int GNM_ERROR_CAPTURE_RAZOR_NOT_LOADED = static_cast<int>(0x80d15001u);
constexpr int GNM_ERROR_CAPTURE_FAILED_INTERNAL = static_cast<int>(0x80d1500fu);

using RegisterValue = std::pair<std::uint32_t, std::uint32_t>;

constexpr std::uint32_t Header(std::uint32_t opcode, std::uint32_t bodyWords, bool compute = false) {
    return 0xc0000000u | ((bodyWords - 1) << 16) | (opcode << 8) | (compute ? 2u : 0u);
}

std::uint32_t* Emit(std::uint32_t* at, std::uint32_t opcode, std::initializer_list<std::uint32_t> body, bool compute = false) {
    *at++ = Header(opcode, static_cast<std::uint32_t>(body.size()), compute);
    for (const auto word : body) *at++ = word;
    return at;
}

std::uint32_t* Nop(std::uint32_t* at, std::uint32_t bodyWords) {
    if (bodyWords == 0) throw std::logic_error("GnmDriver: empty NOP");
    *at = Header(OpNop, bodyWords);
    std::fill_n(at + 1, bodyWords, 0u);
    return at + 1 + bodyWords;
}

std::uint32_t* SetRegisters(std::uint32_t* at, std::uint32_t opcode, std::initializer_list<RegisterValue> values) {
    for (const auto& [reg, value] : values) at = Emit(at, opcode, {reg, value});
    return at;
}

std::uint32_t* DefaultState(std::uint32_t* at, std::uint32_t scModeCntl1) {
    at = SetRegisters(at, OpSetShReg, {{0x216, 0xffffffffu}, {0x217, 0xffffffffu}, {0x215, 0}});
    at = SetRegisters(at, OpSetContextReg, {{0x2f9, 0x2d}, {0x282, 8}, {0x280, 0x80008}, {0x281, 0xffff0000u}, {0x204, 0}, {0x206, 0x43f}, {0x83, 0xffff}, {0x317, 0x10},
        {0x2fa, 0x3f800000u}, {0x2fc, 0x3f800000u}, {0x2fb, 0x3f800000u}, {0x2fd, 0x3f800000u}, {0x202, 0xcc0010}, {0x30e, 0xffffffffu}, {0x30f, 0xffffffffu}});
    at = Emit(at, OpNumInstances, {1});
    at = SetRegisters(at, OpSetShReg, {{7, 0x1ff}, {0x46, 0x1ff}, {0x87, 0x1ff}, {0xc7, 0x1ff}, {0x107, 0}, {0x147, 0x1ff}});
    at = SetRegisters(at, OpSetContextReg, {{0x1b1, 2}, {0x101, 0}, {0x100, 0xffffffffu}, {0x103, 0}, {0x284, 0}, {0x290, 0}, {0x2ae, 0}, {0x292, 0}, {0x293, scModeCntl1},
        {0x2f8, 0}, {0x2de, 0x1e9}});
    at = Emit(at, OpSetContextReg, {0x295, 0x100, 0x100, 4});
    return Emit(at, OpSetUconfigReg, {0x200, 0xe0000000u});
}

struct FrameStart {
    std::uint32_t words[HwInitPacketWords]{};
    std::uint32_t size = 0;
    FrameStart() {
        auto* at = Emit(words, OpClearState, {0});
        size = static_cast<std::uint32_t>(DefaultState(at, 0x6000000) - words);
    }
};

std::mutex g_submitMutex;
bool g_frameStarted = false;
bool g_workQueued = false;

int CheckSubmission(std::uint32_t count, std::uint32_t* const* dcbs, const std::uint32_t* dcbBytes, std::uint32_t* const* ccbs, const std::uint32_t* ccbBytes) {
    if (count != 0 && (!dcbs || !dcbBytes)) return GNM_ERROR_SUBMISSION_FAILED_INVALID_ARGUMENT;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (!dcbs[i] || dcbBytes[i] == 0 || dcbBytes[i] % 4 != 0 || dcbBytes[i] > MaxCommandBufferBytes) return GNM_ERROR_SUBMISSION_FAILED_INVALID_ARGUMENT;
        const auto constantBytes = ccbBytes ? ccbBytes[i] : 0;
        if (constantBytes % 4 != 0 || constantBytes > MaxCommandBufferBytes || (constantBytes != 0 && (!ccbs || !ccbs[i]))) return GNM_ERROR_SUBMISSION_FAILED_INVALID_ARGUMENT;
    }
    return 0;
}

bool IsFiller(std::uint32_t header) {
    return (header >> 30) == 2;
}

}

namespace GnmDriver {

std::span<const std::uint32_t> PacketAt(std::span<const std::uint32_t> words, std::size_t at, const char* stream) {
    const auto header = words[at];
    if (IsFiller(header)) return words.subspan(at, 1);
    if (const auto type = header >> 30; type != 3) {
        char what[256];
        std::snprintf(what, sizeof(what), "GnmDriver: unsupported PM4 packet type %u in the %s: header 0x%08x at dword %zu of %zu%s", type, stream, header, at, words.size(),
            header == 1u ? " (a dword of 1 where a packet header was expected: a title store that landed on a rebuilt buffer; APS5_GNM_SUBMIT_WAIT_IDLE=1 keeps the queue ahead of it)" : "");
        throw std::runtime_error(what);
    }
    const std::size_t total = ((header >> 16) & 0x3fffu) + 2;
    if (total > words.size() - at) throw std::runtime_error(std::string("GnmDriver: truncated packet in the ") + stream);
    return words.subspan(at, total);
}

}

namespace {

std::uint32_t Opcode(std::uint32_t header) {
    return (header >> 8) & 0xffu;
}

void Append(std::vector<std::uint32_t>& out, std::initializer_list<std::uint32_t> words) {
    out.insert(out.end(), words);
}

void Lower(std::vector<std::uint32_t>& out, std::span<const std::uint32_t> packet) {
    switch (Opcode(packet[0])) {
        case OpEventWriteEop:
            if (packet.size() != 6) throw std::runtime_error("GnmDriver: invalid EVENT_WRITE_EOP size");
            Append(out, {Header(OpReleaseMem, 7), packet[1] & EventControlMask, packet[3] & 0xe3000000u, packet[2], packet[3] & 0xffffu, packet[4], packet[5], 0});
            return;
        case OpEventWriteEos:
            if (packet.size() != 5) throw std::runtime_error("GnmDriver: invalid EVENT_WRITE_EOS size");
            if ((packet[3] >> 29) != EosStoreData) NotImplemented_nid_no_patch("EVENT_WRITE_EOS GDS store");
            Append(out, {Header(OpReleaseMem, 7), packet[1] & EventControlMask, ReleaseMemData32, packet[2], packet[3] & 0xffffu, packet[4], 0, 0});
            return;
        case OpSurfaceSync:
            if (packet.size() != 5) throw std::runtime_error("GnmDriver: invalid SURFACE_SYNC size");
            Append(out, {Header(OpAcquireMem, 6), packet[1], packet[2], 0, packet[3], 0, packet[4]});
            return;
        case OpNop:
            out.insert(out.end(), packet.begin(), packet.end());
            if (packet.size() >= 2 && (packet[1] & 0xffff0000u) == 0x68750000u) out[out.size() - packet.size() + 1] = 0;
            return;
        case OpDmaData:
            if (packet.size() == 7 && (packet[6] & DmaRegisterDestination) && (packet[6] & DmaNoDestinationIncrement) && packet[4] == CpDmaReadTagsBytes && packet[5] == 0) {
                const auto at = out.size();
                out.resize(at + packet.size());
                Nop(&out[at], static_cast<std::uint32_t>(packet.size()) - 1);
                return;
            }
            out.insert(out.end(), packet.begin(), packet.end());
            return;
        default:
            out.insert(out.end(), packet.begin(), packet.end());
    }
}

std::uint64_t g_constantCounter = 0;
std::uint64_t g_drawCounter = 0;

}

namespace GnmDriver {

std::vector<std::uint32_t> MergeEngines(std::span<const std::uint32_t> dcb, std::span<const std::uint32_t> ccb) {
    std::vector<std::uint32_t> out;
    out.reserve(dcb.size() + ccb.size());
    std::size_t constant = 0;
    const auto runConstant = [&](bool untilAhead) {
        while (constant < ccb.size() && !(untilAhead && g_constantCounter > g_drawCounter)) {
            const auto packet = PacketAt(ccb, constant, "constant command buffer");
            constant += packet.size();
            if (IsFiller(packet[0])) continue;
            switch (Opcode(packet[0])) {
                case OpIncrementCeCounter:
                    ++g_constantCounter;
                    break;
                case OpWaitOnDeCounterDiff:
                    if (packet.size() != 2) throw std::runtime_error("GnmDriver: invalid WAIT_ON_DE_COUNTER_DIFF size");
                    if (g_constantCounter - g_drawCounter >= packet[1]) NotImplemented_nid_no_patch("a constant engine wait on draw engine progress that has not happened yet");
                    break;
                case OpLoadConstRam: case OpWriteConstRam: case OpDumpConstRam: case OpNop:
                    Lower(out, packet);
                    break;
                default:
                    throw std::runtime_error("GnmDriver: constant engine packet " + std::to_string(Opcode(packet[0])) + " is not implemented");
            }
        }
    };
    for (std::size_t draw = 0; draw < dcb.size();) {
        const auto packet = PacketAt(dcb, draw, "draw command buffer");
        draw += packet.size();
        if (IsFiller(packet[0])) continue;
        switch (Opcode(packet[0])) {
            case OpWaitOnCeCounter:
                runConstant(true);
                if (g_constantCounter <= g_drawCounter) throw std::runtime_error("GnmDriver: the draw engine waits on a constant engine counter that is never incremented");
                break;
            case OpIncrementDeCounter:
                ++g_drawCounter;
                break;
            default:
                Lower(out, packet);
        }
    }
    runConstant(false);
    return out;
}

}

namespace {

void SubmitLocked(std::uint32_t count, std::uint32_t* const* dcbs, const std::uint32_t* dcbBytes, std::uint32_t* const* ccbs, const std::uint32_t* ccbBytes) {
    static FrameStart frameStart;
    static const bool waitIdle = std::getenv("APS5_GNM_SUBMIT_WAIT_IDLE") != nullptr;
    if (waitIdle && g_workQueued) AgcDriverWaitIdle_nid_postfix();
    std::vector<std::vector<std::uint32_t>> merged;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto constantWords = ccbBytes ? ccbBytes[i] / 4 : 0;
        merged.push_back(GnmDriver::MergeEngines({dcbs[i], dcbBytes[i] / 4}, {constantWords != 0 ? ccbs[i] : nullptr, constantWords}));
    }
    if (!g_frameStarted) {
        static const bool gcn = (AgcDriverUseGcnShaders_nid_postfix(), true);
        static_cast<void>(gcn);
        AgcDriverSubmitGraphics_nid_postfix(frameStart.words, frameStart.size);
        g_frameStarted = true;
    }
    for (auto& commands : merged) AgcDriverSubmitGraphics_nid_postfix(commands.data(), static_cast<std::uint32_t>(commands.size()));
    g_workQueued = true;
}

std::uintptr_t BufferLabel(std::uint32_t handle, std::uint32_t index) {
    return (VideoOutBufferLabelAddress_nid_postfix(static_cast<int>(handle)) + index * sizeof(std::uint64_t)) & ~std::uintptr_t{3};
}

void PatchFlip(std::uint32_t* reserve, std::uint32_t handle, std::uint32_t index, std::uint32_t flipMode, std::int64_t flipArg) {
    if (reserve[0] != PrepareFlipHeader) throw std::runtime_error("GnmDriver: the command buffer does not end with a flip reservation");
    const std::uint32_t kind = reserve[1];
    const std::uint32_t labelLow = reserve[2];
    const std::uint32_t labelHigh = reserve[3];
    const std::uint32_t labelValue = reserve[4];
    if (kind != PrepareFlip && kind != PrepareFlipLabel) NotImplemented_nid_no_patch("sceGnmSubmitAndFlipCommandBuffers with an interrupt flip reservation");
    const auto bufferLabel = BufferLabel(handle, index);
    auto* at = Emit(reserve, OpWriteData, {WriteDataToMemory, static_cast<std::uint32_t>(bufferLabel), static_cast<std::uint32_t>(bufferLabel >> 32), 1});
    const auto argument = static_cast<std::uint64_t>(flipArg);
    *at++ = AgcDriver::FlipPacketHeader;
    *at++ = handle;
    *at++ = index;
    *at++ = flipMode;
    *at++ = static_cast<std::uint32_t>(argument);
    *at++ = static_cast<std::uint32_t>(argument >> 32);
    if (kind == PrepareFlipLabel) at = Emit(at, OpWriteData, {WriteDataToMemory, labelLow & ~3u, labelHigh, labelValue});
    Nop(at, static_cast<std::uint32_t>(reserve + FlipReserveWords - at) - 1);
}

std::uint32_t MarkerWords(const char* marker, std::size_t extra) {
    const auto length = std::strlen(marker);
    return static_cast<std::uint32_t>(((length + 8 + extra) >> 2) + ((length + 0xc + extra) >> 3) * 2);
}

void WriteMarker(std::uint32_t* at, std::uint32_t kind, const char* marker, std::uint32_t words, const std::uint32_t* color) {
    const auto length = std::strlen(marker) + 1;
    *at = Header(OpNop, words + 1);
    at[1] = kind;
    auto* text = reinterpret_cast<std::uint8_t*>(at + 2);
    std::memset(text, 0, words * 4);
    std::memcpy(text, marker, length);
    if (color) std::memcpy(text + length + 8, color, sizeof(*color));
}

}

extern "C" {

int APS5_VABI sceGnmSetPsShader(std::uint32_t* cmd, std::uint32_t size, const std::uint32_t* regs) {
    if (!cmd || size <= 0x27) return -1;
    if (!regs) {
        cmd = Emit(cmd, OpSetShReg, {8, 0, 0});
        cmd = Emit(cmd, OpSetContextReg, {0x203, 0});
        Nop(cmd, 0x20);
        return 0;
    }
    if (regs[1] != 0) return -1;
    cmd = Emit(cmd, OpSetShReg, {8, regs[0], 0});
    cmd = Emit(cmd, OpSetShReg, {10, regs[2], regs[3]});
    cmd = Emit(cmd, OpSetContextReg, {0x1c4, regs[4], regs[5]});
    cmd = Emit(cmd, OpSetContextReg, {0x1b3, regs[6], regs[7]});
    cmd = SetRegisters(cmd, OpSetContextReg, {{0x1b6, regs[8]}, {0x1b8, regs[9]}, {0x203, regs[10]}, {0x8f, regs[11]}});
    Nop(cmd, 11);
    return 0;
}

int APS5_VABI sceGnmSetVsShader(std::uint32_t* cmd, std::uint32_t size, const std::uint32_t* regs, std::uint32_t modifier) {
    if (!cmd || size <= 0x1c || !regs || (modifier & 0xfcfffc3fu) || regs[1] != 0) return -1;
    const std::uint32_t rsrc1 = modifier == 0 ? regs[2] : (regs[2] & 0xfcfffc3fu) | modifier;
    cmd = Emit(cmd, OpSetShReg, {0x48, regs[0], 0});
    cmd = Emit(cmd, OpSetShReg, {0x4a, rsrc1, regs[3]});
    cmd = SetRegisters(cmd, OpSetContextReg, {{0x207, regs[6]}, {0x1b1, regs[4]}, {0x1c3, regs[5]}});
    Nop(cmd, 11);
    return 0;
}

int APS5_VABI sceGnmSetCsShader(std::uint32_t* cmd, std::uint32_t size, const std::uint32_t* regs) {
    if (!cmd || size <= 0x18 || !regs || regs[1] != 0) return -1;
    cmd = Emit(cmd, OpSetShReg, {0x20c, regs[0], 0});
    cmd = Emit(cmd, OpSetShReg, {0x212, regs[2], regs[3]});
    cmd = Emit(cmd, OpSetShReg, {0x207, regs[4], regs[5], regs[6]});
    Nop(cmd, 11);
    return 0;
}

int APS5_VABI sceGnmSetEsShader(std::uint32_t* cmd, std::uint32_t size, const std::uint32_t* regs, std::uint32_t modifier) {
    if (!cmd || size < 0x14 || !regs || (modifier & 0xfcfffc3fu) || regs[1] != 0) return -1;
    const std::uint32_t rsrc1 = modifier == 0 ? regs[2] : (regs[2] & 0xfcfffc3fu) | modifier;
    cmd = Emit(cmd, OpSetShReg, {0xc8, regs[0], 0});
    cmd = Emit(cmd, OpSetShReg, {0xca, rsrc1, regs[3]});
    Nop(cmd, 11);
    return 0;
}

int APS5_VABI sceGnmSetGsShader(std::uint32_t* cmd, std::uint32_t size, const std::uint32_t* regs) {
    if (!cmd || size < 0x1d || !regs || regs[1] != 0) return -1;
    cmd = Emit(cmd, OpSetShReg, {0x88, regs[0], 0});
    cmd = Emit(cmd, OpSetShReg, {0x8a, regs[2], regs[3]});
    cmd = SetRegisters(cmd, OpSetContextReg, {{0x2e5, regs[4]}, {0x29b, regs[5]}, {0x2e4, regs[6]}});
    Nop(cmd, 11);
    return 0;
}

int APS5_VABI sceGnmSetHsShader(std::uint32_t* cmd, std::uint32_t size, const std::uint32_t* regs, std::uint32_t lsHsConfig) {
    if (!cmd || size < 0x1e || !regs || regs[1] != 0) return -1;
    cmd = Emit(cmd, OpSetShReg, {0x108, regs[0], 0});
    cmd = Emit(cmd, OpSetShReg, {0x10a, regs[2], regs[3]});
    cmd = Emit(cmd, OpSetContextReg, {0x286, regs[5], regs[6]});
    cmd = SetRegisters(cmd, OpSetContextReg, {{0x2db, regs[4]}, {0x2d6, lsHsConfig}});
    Nop(cmd, 11);
    return 0;
}

int APS5_VABI sceGnmSetLsShader(std::uint32_t* cmd, std::uint32_t size, const std::uint32_t* regs, std::uint32_t modifier) {
    if (!cmd || size < 0x17 || !regs) return -1;
    const std::uint32_t mask = (modifier & 0xfffffc3fu) == 0 ? 0xfffffc3fu : 0xfcfffc3fu;
    if ((modifier & mask) || regs[1] != 0) return -1;
    const std::uint32_t rsrc1 = modifier == 0 ? regs[2] : (regs[2] & mask) | modifier;
    cmd = Emit(cmd, OpSetShReg, {0x148, regs[0], 0});
    cmd = Emit(cmd, OpSetShReg, {0x14b, regs[3]});
    cmd = Emit(cmd, OpSetShReg, {0x14a, rsrc1, regs[3]});
    Nop(cmd, 11);
    return 0;
}

std::uint32_t APS5_VABI sceGnmDrawInitDefaultHardwareState175(std::uint32_t* cmd, std::uint32_t size) {
    if (size < HwInitPacketWords) return 0;
    auto* at = Emit(cmd, OpContextControl, {0x80000000u, 0x80000000u});
    at = Emit(at, OpClearState, {0});
    at = Emit(at, OpAcquireMem, {0x2ec47fc0u, 0xffffffffu, 0, 0, 0, 10});
    at = DefaultState(at, 0x6020000);
    Nop(at, static_cast<std::uint32_t>(cmd + HwInitPacketWords - at) - 1);
    return HwInitPacketWords;
}

std::uint32_t APS5_VABI sceGnmDispatchInitDefaultHardwareState(std::uint32_t* cmd, std::uint32_t size) {
    if (size < HwInitPacketWords) return 0;
    auto* at = Emit(cmd, OpSetShReg, {0x216, 0xffffffffu}, true);
    at = Emit(at, OpSetShReg, {0x217, 0xffffffffu}, true);
    at = Emit(at, OpSetShReg, {0x215, 0x170}, true);
    at = Emit(at, OpAcquireMem, {0x28000000u, 0, 0, 0, 0, 10});
    Nop(at, static_cast<std::uint32_t>(cmd + HwInitPacketWords - at) - 1);
    return HwInitPacketWords;
}

int APS5_VABI sceGnmInsertPushMarker(std::uint32_t* cmd, std::uint32_t size, const char* marker) {
    if (!cmd || !marker) return -1;
    const auto words = MarkerWords(marker, 0);
    if (words + 2 != size) return -1;
    WriteMarker(cmd, MarkerPush, marker, words, nullptr);
    return 0;
}

int APS5_VABI sceGnmInsertSetMarker(std::uint32_t* cmd, std::uint32_t size, const char* marker) {
    if (!cmd || !marker) return -1;
    const auto words = MarkerWords(marker, 0);
    if (words + 2 != size) return -1;
    WriteMarker(cmd, MarkerSet, marker, words, nullptr);
    return 0;
}

int APS5_VABI sceGnmInsertPushColorMarker(std::uint32_t* cmd, std::uint32_t size, const char* marker, std::uint32_t color) {
    if (!cmd || !marker) return -1;
    const auto words = MarkerWords(marker, 4);
    if (words + 2 != size) return -1;
    WriteMarker(cmd, MarkerColorPush, marker, words, &color);
    return 0;
}

int APS5_VABI sceGnmInsertPopMarker(std::uint32_t* cmd, std::uint32_t size) {
    if (!cmd || size != 6) return -1;
    Emit(cmd, OpNop, {MarkerPop, 0, 0, 0, 0});
    return 0;
}

int APS5_VABI sceGnmInsertSetColorMarker() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmInsertWaitFlipDone(std::uint32_t* cmd, std::uint32_t size, std::uint32_t handle, std::uint32_t index) {
    if (size != 7) return -1;
    const auto label = BufferLabel(handle, index);
    Emit(cmd, OpWaitRegMem, {WaitMemoryEqual, static_cast<std::uint32_t>(label), static_cast<std::uint32_t>(label >> 32), 0, 0xffffffffu, 10});
    return 0;
}

int APS5_VABI sceGnmSubmitCommandBuffers(std::uint32_t count, std::uint32_t* const* dcbs, const std::uint32_t* dcbBytes, std::uint32_t* const* ccbs, const std::uint32_t* ccbBytes) {
    if (!dcbs || !dcbBytes) return GNM_ERROR_SUBMISSION_FAILED_INVALID_ARGUMENT;
    if (const int error = CheckSubmission(count, dcbs, dcbBytes, ccbs, ccbBytes)) return error;
    std::lock_guard lock(g_submitMutex);
    SubmitLocked(count, dcbs, dcbBytes, ccbs, ccbBytes);
    return 0;
}

int APS5_VABI sceGnmSubmitAndFlipCommandBuffers(std::uint32_t count, std::uint32_t* const* dcbs, const std::uint32_t* dcbBytes, std::uint32_t* const* ccbs, const std::uint32_t* ccbBytes,
    std::uint32_t handle, std::uint32_t index, std::uint32_t flipMode, std::int64_t flipArg) {
    if (count == 0) return GNM_ERROR_SUBMISSION_FAILED_INVALID_ARGUMENT;
    if (const int error = CheckSubmission(count, dcbs, dcbBytes, ccbs, ccbBytes)) return error;
    if (dcbBytes[count - 1] < FlipReserveWords * 4) throw std::runtime_error("GnmDriver: the last command buffer has no room for a flip reservation");
    std::lock_guard lock(g_submitMutex);
    PatchFlip(dcbs[count - 1] + dcbBytes[count - 1] / 4 - FlipReserveWords, handle, index, flipMode, flipArg);
    SubmitLocked(count, dcbs, dcbBytes, ccbs, ccbBytes);
    return 0;
}

int APS5_VABI sceGnmSubmitDone() {
    std::lock_guard lock(g_submitMutex);
    g_frameStarted = false;
    return 0;
}

int APS5_VABI sceGnmAreSubmitsAllowed() {
    return 1;
}

void APS5_VABI sceGnmFlushGarlic() {
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

int APS5_VABI sceGnmAddEqEvent(KernelEqueue eq, std::uint64_t id, void* udata) {
    if (id != GfxEndOfPipeEvent) NotImplemented_nid_no_patch("sceGnmAddEqEvent for a compute queue event");
    return AgcDriverAddQueueEqEvent_nid_postfix(eq, 0, static_cast<std::uintptr_t>(id), udata);
}

int APS5_VABI sceGnmDeleteEqEvent(KernelEqueue eq, std::uint64_t id) {
    return AgcDriverDeleteQueueEqEvent_nid_postfix(eq, static_cast<std::uintptr_t>(id));
}

int APS5_VABI sceGnmGetEqEventType(const KernelEvent* ev) {
    if (!ev || ev->filter != EvfiltGraphicsCore) throw std::runtime_error(std::string(__func__) + ": not a graphics event");
    return static_cast<int>(ev->ident);
}

bool APS5_VABI sceGnmIsUserPaEnabled() {
    return false;
}

bool APS5_VABI sceGnmDriverCaptureInProgress() {
    return false;
}

int APS5_VABI sceGnmDriverTriggerCapture() {
    return GNM_ERROR_CAPTURE_RAZOR_NOT_LOADED;
}

int APS5_VABI sceGnmValidateCommandBuffers() {
    return GNM_ERROR_VALIDATION_NOT_ENABLED;
}

bool APS5_VABI sceRazorIsLoaded() {
    return false;
}

int APS5_VABI sceRazorCaptureImmediate() {
    return GNM_ERROR_CAPTURE_FAILED_INTERNAL;
}

int APS5_VABI sceRazorCaptureSinceLastFlip() {
    return GNM_ERROR_CAPTURE_FAILED_INTERNAL;
}

int APS5_VABI sceRazorCaptureCommandBuffersOnlyImmediate() {
    return GNM_ERROR_CAPTURE_FAILED_INTERNAL;
}

int APS5_VABI sceRazorCaptureCommandBuffersOnlySinceLastFlip() {
    return GNM_ERROR_CAPTURE_FAILED_INTERNAL;
}

int APS5_VABI sceGnmDebugHardwareStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDingDong() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDisableMipStatsReport() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetEqTimeStamp() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI sceGnmGetTheTessellationFactorRingBufferBaseAddress() {
    return 0xFF0000000ull;
}

int APS5_VABI sceGnmLogicalCuIndexToPhysicalCuIndex() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmLogicalCuMaskToPhysicalCuMask() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmMapComputeQueue() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmRequestFlipAndSubmitDone() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmRequestMipStatsReportAndReset() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmSetGsRingSizes() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmSetupMipStatsReport() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmUnmapComputeQueue() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {
APS5_EXPORT("+AFvOEXrKJk", Aps5LocalStub_libSceGnmDriver_0_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_0_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmSetEmbeddedVsShader [libSceGnmDriver +AFvOEXrKJk]");
    return 0;
}
APS5_EXPORT("0BzLGljcwBo", Aps5LocalStub_libSceGnmDriver_1_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_1_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmDispatchDirect [libSceGnmDriver 0BzLGljcwBo]");
    return 0;
}
APS5_EXPORT("4Mv9OXypBG8", Aps5LocalStub_libSceGnmDriver_2_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_2_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmFindResourcesPublic [libSceGnmDriver 4Mv9OXypBG8]");
    return 0;
}
APS5_EXPORT("4PKnYXOhcx4", Aps5LocalStub_libSceGnmDriver_3_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_3_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmGetResourceBaseAddressAndSizeInBytes [libSceGnmDriver 4PKnYXOhcx4]");
    return 0;
}
APS5_EXPORT("4v+otIIdjqg", Aps5LocalStub_libSceGnmDriver_4_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_4_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmDrawIndirect [libSceGnmDriver 4v+otIIdjqg]");
    return 0;
}
APS5_EXPORT("5SHGNwLXBV4", Aps5LocalStub_libSceGnmDriver_5_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_5_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmValidateGetDiagnostics [libSceGnmDriver 5SHGNwLXBV4]");
    return 0;
}
APS5_EXPORT("5uFKckiJYRM", Aps5LocalStub_libSceGnmDriver_6_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_6_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmSetPsShader350 [libSceGnmDriver 5uFKckiJYRM]");
    return 0;
}
APS5_EXPORT("5udAm+6boVg", Aps5LocalStub_libSceGnmDriver_7_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_7_nid_no_patch(const char*, std::uint32_t* stream) {
    if (stream) *stream = 1;
    return 0;
}
APS5_EXPORT("6IMbpR7nTzA", Aps5LocalStub_libSceGnmDriver_8_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_8_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmQueryResourceRegistrationUserMemoryRequirements [libSceGnmDriver 6IMbpR7nTzA]");
    return 0;
}
APS5_EXPORT("6YRHhh5mHCs", Aps5LocalStub_libSceGnmDriver_9_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_9_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmRequestFlipAndSubmitDoneForWorkload [libSceGnmDriver 6YRHhh5mHCs]");
    return 0;
}
APS5_EXPORT("9Mv61HaMhfA", Aps5LocalStub_libSceGnmDriver_10_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_10_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmRegisterGdsResource [libSceGnmDriver 9Mv61HaMhfA]");
    return 0;
}
APS5_EXPORT("A+uGq+3KFtQ", Aps5LocalStub_libSceGnmDriver_11_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_11_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmMapComputeQueueWithPriority [libSceGnmDriver A+uGq+3KFtQ]");
    return 0;
}
APS5_EXPORT("BgM3t3LvcNk", Aps5LocalStub_libSceGnmDriver_12_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_12_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmValidateDisableDiagnostics2 [libSceGnmDriver BgM3t3LvcNk]");
    return 0;
}
APS5_EXPORT("ED9-Fjr8Ta4", Aps5LocalStub_libSceGnmDriver_13_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_13_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmDrawIndexIndirect [libSceGnmDriver ED9-Fjr8Ta4]");
    return 0;
}
APS5_EXPORT("EJapNl2+pgU", Aps5LocalStub_libSceGnmDriver_14_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_14_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmComputeWaitSemaphore [libSceGnmDriver EJapNl2+pgU]");
    return 0;
}
APS5_EXPORT("FFVZcCu3zWU", Aps5LocalStub_libSceGnmDriver_15_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_15_nid_no_patch(void) {
    return 0x800000;
}
APS5_EXPORT("Fa3x75OOLRA", Aps5LocalStub_libSceGnmDriver_16_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_16_nid_no_patch(std::uint64_t) {
    return 0;
}
APS5_EXPORT("GGsn7jMTxw4", Aps5LocalStub_libSceGnmDriver_17_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_17_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmDrawIndexAuto [libSceGnmDriver GGsn7jMTxw4]");
    return 0;
}
APS5_EXPORT("GNlx+y7xPdE", Aps5LocalStub_libSceGnmDriver_18_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_18_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmUpdateHsShader [libSceGnmDriver GNlx+y7xPdE]");
    return 0;
}
APS5_EXPORT("Ga6r7H6Y0RI", Aps5LocalStub_libSceGnmDriver_19_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_19_nid_no_patch(std::uint64_t, std::uint32_t count, std::uint32_t* const* dcbs, const std::uint32_t* dcbBytes, std::uint32_t* const* ccbs, const std::uint32_t* ccbBytes, std::uint32_t handle, std::uint32_t index, std::uint32_t flipMode, std::int64_t flipArg) {
    return sceGnmSubmitAndFlipCommandBuffers(count, dcbs, dcbBytes, ccbs, ccbBytes, handle, index, flipMode, flipArg);
}
APS5_EXPORT("HlTPoZ-oY7Y", Aps5LocalStub_libSceGnmDriver_20_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_20_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmDrawIndex [libSceGnmDriver HlTPoZ-oY7Y]");
    return 0;
}
APS5_EXPORT("HzMN7ANqYEc", Aps5LocalStub_libSceGnmDriver_21_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_21_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmValidateGetVersion [libSceGnmDriver HzMN7ANqYEc]");
    return 0;
}
APS5_EXPORT("K3BKBBYKUSE", Aps5LocalStub_libSceGnmDriver_22_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_22_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmSetResourceUserData [libSceGnmDriver K3BKBBYKUSE]");
    return 0;
}
APS5_EXPORT("Kl0Z3LH07QI", Aps5LocalStub_libSceGnmDriver_23_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_23_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmLogicalTcaUnitToPhysical [libSceGnmDriver Kl0Z3LH07QI]");
    return 0;
}
APS5_EXPORT("KnldROUkWJY", Aps5LocalStub_libSceGnmDriver_24_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_24_nid_no_patch(void) {
    return 1;
}
APS5_EXPORT("Kx-h-nWQJ8A", Aps5LocalStub_libSceGnmDriver_25_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_25_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmSetCsShaderWithModifier [libSceGnmDriver Kx-h-nWQJ8A]");
    return 0;
}
APS5_EXPORT("MBMa6EFu4Ko", Aps5LocalStub_libSceGnmDriver_26_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_26_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmValidateResetState [libSceGnmDriver MBMa6EFu4Ko]");
    return 0;
}
APS5_EXPORT("MYRtYhojKdA", Aps5LocalStub_libSceGnmDriver_27_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_27_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmResetVgtControl [libSceGnmDriver MYRtYhojKdA]");
    return 0;
}
APS5_EXPORT("NfvOrNzy6sk", Aps5LocalStub_libSceGnmDriver_28_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_28_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmInsertDingDongMarker [libSceGnmDriver NfvOrNzy6sk]");
    return 0;
}
APS5_EXPORT("O0S96YnD04U", Aps5LocalStub_libSceGnmDriver_29_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_29_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmGetResourceName [libSceGnmDriver O0S96YnD04U]");
    return 0;
}
APS5_EXPORT("Q7t4VEYLafI", Aps5LocalStub_libSceGnmDriver_30_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_30_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmValidationRegisterMemoryCheckCallback [libSceGnmDriver Q7t4VEYLafI]");
    return 0;
}
APS5_EXPORT("QJjPjlmPAL0", Aps5LocalStub_libSceGnmDriver_31_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_31_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmGetOwnerName [libSceGnmDriver QJjPjlmPAL0]");
    return 0;
}
APS5_EXPORT("R6z1xM3pW-w", Aps5LocalStub_libSceGnmDriver_32_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_32_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmDriverTraceInProgress [libSceGnmDriver R6z1xM3pW-w]");
    return 0;
}
APS5_EXPORT("RX7XCNSaL6I", Aps5LocalStub_libSceGnmDriver_33_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_33_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmValidateGetDiagnosticInfo [libSceGnmDriver RX7XCNSaL6I]");
    return 0;
}
APS5_EXPORT("SXw4dZEkgpA", Aps5LocalStub_libSceGnmDriver_34_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_34_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmValidateDisableDiagnostics [libSceGnmDriver SXw4dZEkgpA]");
    return 0;
}
APS5_EXPORT("UBv7FkVfzcQ", Aps5LocalStub_libSceGnmDriver_35_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_35_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmGetResourceShaderGuid [libSceGnmDriver UBv7FkVfzcQ]");
    return 0;
}
APS5_EXPORT("URDgJcXhQOs", Aps5LocalStub_libSceGnmDriver_36_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_36_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmInsertThreadTraceMarker [libSceGnmDriver URDgJcXhQOs]");
    return 0;
}
APS5_EXPORT("UoBuWAhKk7U", Aps5LocalStub_libSceGnmDriver_37_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_37_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmGetResourceUserData [libSceGnmDriver UoBuWAhKk7U]");
    return 0;
}
APS5_EXPORT("UtObDRQiGbs", Aps5LocalStub_libSceGnmDriver_38_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_38_nid_no_patch(std::uint32_t) {
    return 0;
}
APS5_EXPORT("V31V01UiScY", Aps5LocalStub_libSceGnmDriver_39_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_39_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmUpdateVsShader [libSceGnmDriver V31V01UiScY]");
    return 0;
}
APS5_EXPORT("X9Omw9dwv5M", Aps5LocalStub_libSceGnmDriver_40_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_40_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmSetEmbeddedPsShader [libSceGnmDriver X9Omw9dwv5M]");
    return 0;
}
APS5_EXPORT("XiyzNZ9J4nQ", Aps5LocalStub_libSceGnmDriver_41_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_41_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmSetWaveLimitMultipliers [libSceGnmDriver XiyzNZ9J4nQ]");
    return 0;
}
APS5_EXPORT("Z43vKp5k7r0", Aps5LocalStub_libSceGnmDriver_42_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_42_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmDispatchIndirect [libSceGnmDriver Z43vKp5k7r0]");
    return 0;
}
APS5_EXPORT("ZFqKFl23aMc", Aps5LocalStub_libSceGnmDriver_43_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_43_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmRegisterOwner [libSceGnmDriver ZFqKFl23aMc]");
    return 0;
}
APS5_EXPORT("bdqdvIkLPIU", Aps5LocalStub_libSceGnmDriver_44_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_44_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmGetResourceType [libSceGnmDriver bdqdvIkLPIU]");
    return 0;
}
APS5_EXPORT("byXlqupd8cE", Aps5LocalStub_libSceGnmDriver_45_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_45_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmDingDongForWorkload [libSceGnmDriver byXlqupd8cE]");
    return 0;
}
APS5_EXPORT("cFCp0NX8wf0", Aps5LocalStub_libSceGnmDriver_46_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_46_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmSetVgtControl [libSceGnmDriver cFCp0NX8wf0]");
    return 0;
}
APS5_EXPORT("cUCo8OvArrw", Aps5LocalStub_libSceGnmDriver_47_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_47_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmDrawIndirectCountMulti [libSceGnmDriver cUCo8OvArrw]");
    return 0;
}
APS5_EXPORT("dewXw5roLs0", Aps5LocalStub_libSceGnmDriver_48_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_48_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmGetPhysicalCounterFromVirtualized [libSceGnmDriver dewXw5roLs0]");
    return 0;
}
APS5_EXPORT("ffrNQOshows", Aps5LocalStub_libSceGnmDriver_49_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_49_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmComputeWaitOnAddress [libSceGnmDriver ffrNQOshows]");
    return 0;
}
APS5_EXPORT("fhKwCVVj9nk", Aps5LocalStub_libSceGnmDriver_50_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_50_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmUnregisterOwnerAndResources [libSceGnmDriver fhKwCVVj9nk]");
    return 0;
}
APS5_EXPORT("fzJdEihTFV4", Aps5LocalStub_libSceGnmDriver_51_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_51_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmGetProtectionFaultTimeStamp [libSceGnmDriver fzJdEihTFV4]");
    return 0;
}
APS5_EXPORT("hsZPf1lON7E", Aps5LocalStub_libSceGnmDriver_52_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_52_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmValidateDrawCommandBuffers [libSceGnmDriver hsZPf1lON7E]");
    return 0;
}
APS5_EXPORT("iFirFzgYsvw", Aps5LocalStub_libSceGnmDriver_53_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_53_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmGetLastWaitedAddress [libSceGnmDriver iFirFzgYsvw]");
    return 0;
}
APS5_EXPORT("ihxrbsoSKWc", Aps5LocalStub_libSceGnmDriver_54_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_54_nid_no_patch(std::uint32_t stream, std::uint64_t* workload) {
    const bool known = stream == 1;
    if (workload) *workload = known ? 1 : 0;
    return known ? 0 : 1;
}
APS5_EXPORT("im2ZuItabu4", Aps5LocalStub_libSceGnmDriver_55_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_55_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmDrawInitToDefaultContextState400 [libSceGnmDriver im2ZuItabu4]");
    return 0;
}
APS5_EXPORT("jHdPvIzlpKc", Aps5LocalStub_libSceGnmDriver_56_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_56_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmDrawIndexMultiInstanced [libSceGnmDriver jHdPvIzlpKc]");
    return 0;
}
APS5_EXPORT("jRcI8VcgTz4", Aps5LocalStub_libSceGnmDriver_57_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_57_nid_no_patch(std::uint64_t, std::uint32_t count, std::uint32_t* const* dcbs, const std::uint32_t* dcbBytes, std::uint32_t* const* ccbs, const std::uint32_t* ccbBytes) {
    return sceGnmSubmitCommandBuffers(count, dcbs, dcbBytes, ccbs, ccbBytes);
}
APS5_EXPORT("k7iGTvDQPLQ", Aps5LocalStub_libSceGnmDriver_58_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_58_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmGetShaderStatus [libSceGnmDriver k7iGTvDQPLQ]");
    return 0;
}
APS5_EXPORT("k8EXkhIP+lM", Aps5LocalStub_libSceGnmDriver_59_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_59_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmUnregisterResource [libSceGnmDriver k8EXkhIP+lM]");
    return 0;
}
APS5_EXPORT("mLVL7N7BVBg", Aps5LocalStub_libSceGnmDriver_60_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_60_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmUpdatePsShader350 [libSceGnmDriver mLVL7N7BVBg]");
    return 0;
}
APS5_EXPORT("nEyFbYUloIM", Aps5LocalStub_libSceGnmDriver_61_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_61_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmGetShaderProgramBaseAddress [libSceGnmDriver nEyFbYUloIM]");
    return 0;
}
APS5_EXPORT("nLM2i2+65hA", Aps5LocalStub_libSceGnmDriver_62_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_62_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmUpdateGsShader [libSceGnmDriver nLM2i2+65hA]");
    return 0;
}
APS5_EXPORT("nvEwfYAImTs", Aps5LocalStub_libSceGnmDriver_63_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_63_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmRegisterResource [libSceGnmDriver nvEwfYAImTs]");
    return 0;
}
APS5_EXPORT("oL4hGI1PMpw", Aps5LocalStub_libSceGnmDriver_64_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_64_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmGetGpuBlockStatus [libSceGnmDriver oL4hGI1PMpw]");
    return 0;
}
APS5_EXPORT("oYM+YzfCm2Y", Aps5LocalStub_libSceGnmDriver_65_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_65_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmDrawIndexOffset [libSceGnmDriver oYM+YzfCm2Y]");
    return 0;
}
APS5_EXPORT("q-qhDxP67Hg", Aps5LocalStub_libSceGnmDriver_66_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_66_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmSetResourceRegistrationUserMemory [libSceGnmDriver q-qhDxP67Hg]");
    return 0;
}
APS5_EXPORT("qGP74T5OWJc", Aps5LocalStub_libSceGnmDriver_67_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_67_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmValidateDispatchCommandBuffers [libSceGnmDriver qGP74T5OWJc]");
    return 0;
}
APS5_EXPORT("rTIV11nMQuM", Aps5LocalStub_libSceGnmDriver_68_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_68_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmValidateOnSubmitEnabled [libSceGnmDriver rTIV11nMQuM]");
    return 0;
}
APS5_EXPORT("stDSYW2SBVM", Aps5LocalStub_libSceGnmDriver_69_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_69_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmDrawOpaqueAuto [libSceGnmDriver stDSYW2SBVM]");
    return 0;
}
APS5_EXPORT("tZCSL5ulnB4", Aps5LocalStub_libSceGnmDriver_70_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_70_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmGetGpuInfoStatus [libSceGnmDriver tZCSL5ulnB4]");
    return 0;
}
APS5_EXPORT("thbPcG7E7qk", Aps5LocalStub_libSceGnmDriver_71_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_71_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmDrawIndexIndirectCountMulti [libSceGnmDriver thbPcG7E7qk]");
    return 0;
}
APS5_EXPORT("wED4ZXCFJT0", Aps5LocalStub_libSceGnmDriver_72_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_72_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmDispatchIndirectOnMec [libSceGnmDriver wED4ZXCFJT0]");
    return 0;
}
APS5_EXPORT("yb2cRhagD1I", Aps5LocalStub_libSceGnmDriver_73_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_73_nid_no_patch(std::uint32_t* cmd, std::uint32_t size) {
    if (size < HwInitPacketWords) return 0;
    auto* at = Emit(cmd, OpContextControl, {0x80000000u, 0x80000000u});
    at = Emit(at, OpClearState, {0});
    at = Emit(at, OpAcquireMem, {0x2ec47fc0u, 0xffffffffu, 0, 0, 0, 10});
    at = SetRegisters(at, OpSetShReg, {{0x216, 0xffffffffu}, {0x217, 0xffffffffu}, {0x215, 0}});
    at = SetRegisters(at, OpSetContextReg, {{0x2f9, 0x2d}, {0x282, 8}, {0x280, 0x80008}, {0x281, 0xffff0000u}, {0x204, 0}, {0x206, 0x43f}, {0x83, 0xffff}, {0x317, 0x10},
        {0x2fa, 0x3f800000u}, {0x2fc, 0x3f800000u}, {0x2fb, 0x3f800000u}, {0x2fd, 0x3f800000u}, {0x202, 0xcc0010}, {0x30e, 0xffffffffu}, {0x30f, 0xffffffffu}});
    at = Emit(at, OpNumInstances, {1});
    at = SetRegisters(at, OpSetShReg, {{7, 0x1701ff}, {0x46, 0x1701fd}, {0x87, 0x1701ff}, {0xc7, 0x1701fd}, {0x107, 0x17}, {0x147, 0x1701fd}, {0x47, 0x1c}});
    at = SetRegisters(at, OpSetContextReg, {{0x1b1, 2}, {0x101, 0}, {0x100, 0xffffffffu}, {0x103, 0}, {0x284, 0}, {0x290, 0}, {0x2ae, 0}, {0x102, 0}, {0x292, 0}, {0x293, 0x6020000},
        {0x2f8, 0}, {0x2de, 0x1e9}});
    at = Emit(at, OpSetContextReg, {0x295, 0x100, 0x100, 4});
    at = Emit(at, OpSetUconfigReg, {0x200, 0xe0000000u});
    at = Emit(at, OpSetContextReg, {0x2aa, 0xff});
    Nop(at, static_cast<std::uint32_t>(cmd + HwInitPacketWords - at) - 1);
    return HwInitPacketWords;
}
APS5_EXPORT("yhFCnaz5daw", Aps5LocalStub_libSceGnmDriver_74_nid_no_patch);
std::int64_t APS5_VABI Aps5LocalStub_libSceGnmDriver_74_nid_no_patch(void) {
    NotImplemented_nid_no_patch("sceGnmUnregisterAllResourcesForOwner [libSceGnmDriver yhFCnaz5daw]");
    return 0;
}
}
