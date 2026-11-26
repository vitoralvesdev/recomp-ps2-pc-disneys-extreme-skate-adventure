#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>

#ifdef PS2_FUNCTION_LOG_TRACKER
#include "ps2_log.h"
#endif

namespace {

constexpr uint32_t kDecompressEntry = 0x270f58u;
constexpr uint32_t kDictionaryInitResume = 0x270f70u;
constexpr uint32_t kLiteralResume = 0x270fa8u;
constexpr uint32_t kMatchResume = 0x270fccu;
constexpr uint32_t kMatchCopyResume = 0x271008u;
constexpr uint32_t kControlResume = 0x27103cu;
constexpr uint32_t kLiteralBranchResume = 0x271064u;
constexpr uint32_t kReturnPc = 0x271074u;
constexpr uint32_t kDictionaryBase = 0x003f9578u;
constexpr uint32_t kCheckpointBytes = 1024u;

bool cdLoadTraceEnabled()
{
    static const bool enabled = []
    {
        const char *value = std::getenv("PS2X_CD_LOAD_TRACE");
        return value != nullptr && value[0] != '\0' && value[0] != '0';
    }();
    return enabled;
}

void traceDecompressSlice(std::chrono::steady_clock::time_point started,
                          uint64_t outputBytes,
                          uint32_t remainingBytes,
                          bool completed)
{
    if (!cdLoadTraceEnabled())
    {
        return;
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
                                 std::chrono::steady_clock::now() - started)
                                 .count();
    std::fprintf(stderr,
                 "[cd-load] stage=decompress elapsed_ms=%.3f output_bytes=%llu "
                 "remaining_bytes=%u completed=%d\n",
                 elapsedMs,
                 static_cast<unsigned long long>(outputBytes),
                 remainingBytes,
                 completed ? 1 : 0);
}

void runLoadDecompress(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    const bool traceEnabled = cdLoadTraceEnabled();
    const auto traceStart = traceEnabled ? std::chrono::steady_clock::now()
                                         : std::chrono::steady_clock::time_point{};
    uint32_t source = GPR_U32(ctx, 4);
    uint32_t destination = GPR_U32(ctx, 5);
    uint32_t remaining = GPR_U32(ctx, 6);
    uint32_t controlBits = GPR_U32(ctx, 13);
    uint32_t dictionaryWrite = GPR_U32(ctx, 10);
    uint32_t matchOffset = 0u;
    uint32_t matchIndex = 0u;
    uint32_t matchLength = 0u;
    uint32_t uncheckpointedBytes = 0u;
    uint64_t outputBytes = 0u;

    const auto saveState = [&]
    {
        SET_GPR_S32(ctx, 4, static_cast<int32_t>(source));
        SET_GPR_S32(ctx, 5, static_cast<int32_t>(destination));
        SET_GPR_S32(ctx, 6, static_cast<int32_t>(remaining));
        SET_GPR_U64(ctx, 10, dictionaryWrite);
        SET_GPR_U64(ctx, 12, kDictionaryBase);
        SET_GPR_U64(ctx, 13, controlBits);
        SET_GPR_U64(ctx, 14, kDictionaryBase);
        SET_GPR_U64(ctx, 15, 0x00400000u);
        ctx->in_delay_slot = false;
    };

    const auto finish = [&]
    {
        if (uncheckpointedBytes != 0u)
        {
            (void)runtime->eeCheckpointDue(uncheckpointedBytes * 32u);
        }
        saveState();
        const uint32_t returnTarget = GPR_U32(ctx, 31);
        ctx->pc = returnTarget;
#if defined(PS2X_STRICT_RETURN_DIAGNOSTICS) && PS2X_STRICT_RETURN_DIAGNOSTICS
        (void)runtime->dispatchGuestBranch(rdram, ctx, returnTarget, kReturnPc, 0u,
                                           PS2Runtime::GuestBranchKind::Return,
                                           "JR $ra");
#endif
        if (traceEnabled)
        {
            traceDecompressSlice(traceStart, outputBytes, remaining, true);
        }
    };

    const auto checkpoint = [&]() -> bool
    {
        if (uncheckpointedBytes < kCheckpointBytes)
        {
            return false;
        }
        const uint32_t cycles = uncheckpointedBytes * 32u;
        uncheckpointedBytes = 0u;
        if (!runtime->eeCheckpointDue(cycles))
        {
            return false;
        }
        saveState();
        ctx->pc = kControlResume;
        if (traceEnabled)
        {
            traceDecompressSlice(traceStart, outputBytes, remaining, false);
        }
        return true;
    };

    const auto loadMatch = [&]
    {
        const uint32_t first = READ8(source);
        const uint32_t second = READ8(source + 1u);
        source += 2u;
        matchOffset = first | ((second & 0xf0u) << 4);
        matchLength = (second & 0x0fu) + 3u;
        matchIndex = 0u;
    };

    const auto startMatch = [&]() -> bool
    {
        if (remaining == 0u)
        {
            return false;
        }
        remaining -= 2u;
        loadMatch();
        return true;
    };

    const auto copyMatch = [&]() -> bool
    {
        const uint32_t initialIndex = matchIndex;
        while (matchIndex < matchLength)
        {
            const uint32_t readAddress = kDictionaryBase +
                                         ((matchOffset + matchIndex) & 0xfffu);
            const uint8_t value = READ8(readAddress);
            WRITE8(destination, value);
            WRITE8(kDictionaryBase + dictionaryWrite, value);
            dictionaryWrite = (dictionaryWrite + 1u) & 0xfffu;
            ++destination;
            ++matchIndex;
        }
        const uint32_t produced = matchLength - initialIndex;
        outputBytes += produced;
        uncheckpointedBytes += produced;
        return checkpoint();
    };

    const auto copyLiteral = [&]() -> bool
    {
        const uint8_t value = READ8(source);
        ++source;
        --remaining;
        WRITE8(destination, value);
        WRITE8(kDictionaryBase + dictionaryWrite, value);
        dictionaryWrite = (dictionaryWrite + 1u) & 0xfffu;
        ++destination;
        ++outputBytes;
        ++uncheckpointedBytes;
        return checkpoint();
    };

    switch (ctx->pc)
    {
    case kDecompressEntry:
    default:
        for (uint32_t index = 0u; index < 0xfeeu; ++index)
        {
            WRITE8(kDictionaryBase + index, 0x20u);
        }
        source = GPR_U32(ctx, 4);
        destination = GPR_U32(ctx, 5);
        remaining = GPR_U32(ctx, 6);
        controlBits = 0u;
        dictionaryWrite = 0xfeeu;
        if (runtime->eeCheckpointDue(0xfeeu * 32u))
        {
            saveState();
            ctx->pc = kControlResume;
            if (traceEnabled)
            {
                traceDecompressSlice(traceStart, outputBytes, remaining, false);
            }
            return;
        }
        break;

    case kDictionaryInitResume:
    {
        uint32_t address = GPR_U32(ctx, 2);
        int32_t count = GPR_S32(ctx, 9);
        while (true)
        {
            WRITE8(address, static_cast<uint8_t>(GPR_U32(ctx, 3)));
            --count;
            if (count < 0)
            {
                break;
            }
            --address;
        }
        controlBits = 0u;
        dictionaryWrite = 0xfeeu;
        break;
    }

    case kLiteralResume:
    {
        const uint32_t oldDictionaryWrite = GPR_U32(ctx, 10);
        const uint8_t value = static_cast<uint8_t>(GPR_U32(ctx, 7));
        WRITE8(destination, value);
        WRITE8(kDictionaryBase + oldDictionaryWrite, value);
        dictionaryWrite = (oldDictionaryWrite + 1u) & 0xfffu;
        ++source;
        --remaining;
        ++destination;
        ++outputBytes;
        ++uncheckpointedBytes;
        if (checkpoint())
        {
            return;
        }
        break;
    }

    case kMatchResume:
    {
        const uint32_t oldRemaining = remaining;
        remaining -= 2u;
        if (oldRemaining == 0u)
        {
            finish();
            return;
        }
        loadMatch();
        if (copyMatch())
        {
            return;
        }
        break;
    }

    case kMatchCopyResume:
        matchOffset = GPR_U32(ctx, 9);
        matchIndex = GPR_U32(ctx, 11);
        matchLength = GPR_U32(ctx, 8);
        dictionaryWrite = GPR_U32(ctx, 10);
        if (copyMatch())
        {
            return;
        }
        break;

    case kControlResume:
        break;

    case kLiteralBranchResume:
        if ((GPR_U32(ctx, 2) & 1u) == 0u)
        {
            if (!startMatch())
            {
                finish();
                return;
            }
            if (copyMatch())
            {
                return;
            }
        }
        else
        {
            if (remaining == 0u)
            {
                finish();
                return;
            }
            if (copyLiteral())
            {
                return;
            }
        }
        break;

    case kReturnPc:
        finish();
        return;
    }

    while (true)
    {
        controlBits >>= 1;
        if ((controlBits & 0x100u) == 0u)
        {
            if (remaining == 0u)
            {
                finish();
                return;
            }
            --remaining;
            controlBits = static_cast<uint32_t>(READ8(source)) | 0xff00u;
            ++source;
        }

        if ((controlBits & 1u) != 0u)
        {
            if (remaining == 0u)
            {
                finish();
                return;
            }
            if (copyLiteral())
            {
                return;
            }
        }
        else
        {
            if (!startMatch())
            {
                finish();
                return;
            }
            if (copyMatch())
            {
                return;
            }
        }
    }
}

} // namespace

void FUN_00270f58_0x270f58(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("FUN_00270f58_0x270f58");
#endif
    runLoadDecompress(rdram, ctx, runtime);
}

void entry_00270f70_0x270f70(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("entry_00270f70_0x270f70");
#endif
    ctx->pc = kDictionaryInitResume;
    runLoadDecompress(rdram, ctx, runtime);
}

void entry_00270fa8_0x270fa8(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("entry_00270fa8_0x270fa8");
#endif
    ctx->pc = kLiteralResume;
    runLoadDecompress(rdram, ctx, runtime);
}

void entry_00270fcc_0x270fcc(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("entry_00270fcc_0x270fcc");
#endif
    ctx->pc = kMatchResume;
    runLoadDecompress(rdram, ctx, runtime);
}

void entry_00271008_0x271008(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("entry_00271008_0x271008");
#endif
    ctx->pc = kMatchCopyResume;
    runLoadDecompress(rdram, ctx, runtime);
}

void entry_0027103c_0x27103c(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("entry_0027103c_0x27103c");
#endif
    ctx->pc = kControlResume;
    runLoadDecompress(rdram, ctx, runtime);
}

void entry_00271064_0x271064(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
#ifdef PS2_FUNCTION_LOG_TRACKER
    PS_LOG_ENTRY("entry_00271064_0x271064");
#endif
    ctx->pc = kLiteralBranchResume;
    runLoadDecompress(rdram, ctx, runtime);
}
