#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include "runtime/ee_scheduler.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

void FUN_00270f58_0x270f58(uint8_t *, R5900Context *, PS2Runtime *);
void entry_00270f70_0x270f70(uint8_t *, R5900Context *, PS2Runtime *);
void entry_00270fa8_0x270fa8(uint8_t *, R5900Context *, PS2Runtime *);
void entry_00270fcc_0x270fcc(uint8_t *, R5900Context *, PS2Runtime *);
void entry_00271008_0x271008(uint8_t *, R5900Context *, PS2Runtime *);
void entry_0027103c_0x27103c(uint8_t *, R5900Context *, PS2Runtime *);
void entry_00271064_0x271064(uint8_t *, R5900Context *, PS2Runtime *);

const uint32_t g_ps2RecompiledFunctionTableBase = 0x1000u;
const uint32_t g_ps2RecompiledFunctionTableEnd = 0x1040u;
const uint32_t g_ps2RecompiledFunctionTableSlotCount = 16u;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[16] = {};

namespace {

constexpr uint32_t kSource = 0x00100000u;
constexpr uint32_t kDestination = 0x00800000u;
constexpr uint32_t kDictionary = 0x003f9578u;
constexpr uint32_t kReturnTarget = 0x00200000u;

void check(bool condition, const std::string &label)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", label.c_str());
        std::exit(1);
    }
}

class StreamBuilder
{
public:
    void literal(uint8_t value)
    {
        payload_.push_back(value);
        flags_ |= static_cast<uint8_t>(1u << count_);
        ++count_;
        flushIfFull();
    }

    void match(uint16_t offset, uint8_t length)
    {
        payload_.push_back(static_cast<uint8_t>(offset & 0xffu));
        payload_.push_back(static_cast<uint8_t>(((offset >> 4) & 0xf0u) |
                                                ((length - 3u) & 0x0fu)));
        ++count_;
        flushIfFull();
    }

    std::vector<uint8_t> finish()
    {
        if (count_ != 0u)
        {
            bytes_.push_back(flags_);
            bytes_.insert(bytes_.end(), payload_.begin(), payload_.end());
        }
        return std::move(bytes_);
    }

private:
    void flushIfFull()
    {
        if (count_ == 8u)
        {
            bytes_.push_back(flags_);
            bytes_.insert(bytes_.end(), payload_.begin(), payload_.end());
            flags_ = 0u;
            payload_.clear();
            count_ = 0u;
        }
    }

    std::vector<uint8_t> bytes_;
    std::vector<uint8_t> payload_;
    uint8_t flags_ = 0u;
    uint8_t count_ = 0u;
};

struct ReferenceResult
{
    std::vector<uint8_t> memory;
    std::vector<uint8_t> output;
};

ReferenceResult decodeReference(const std::vector<uint8_t> &base,
                                const std::vector<uint8_t> &input)
{
    ReferenceResult result{base, {}};
    auto &memory = result.memory;
    const auto read = [&](uint32_t address) -> uint8_t
    {
        return memory[address & (PS2_RAM_SIZE - 1u)];
    };
    const auto write = [&](uint32_t address, uint8_t value)
    {
        memory[address & (PS2_RAM_SIZE - 1u)] = value;
    };

    for (uint32_t index = 0u; index < 0xfeeu; ++index)
    {
        write(kDictionary + index, 0x20u);
    }

    uint32_t source = kSource;
    uint32_t destination = kDestination;
    uint32_t remaining = static_cast<uint32_t>(input.size());
    uint32_t controlBits = 0u;
    uint32_t dictionaryWrite = 0xfeeu;

    while (true)
    {
        controlBits >>= 1;
        if ((controlBits & 0x100u) == 0u)
        {
            if (remaining == 0u)
            {
                break;
            }
            --remaining;
            controlBits = static_cast<uint32_t>(read(source++)) | 0xff00u;
        }

        if ((controlBits & 1u) != 0u)
        {
            if (remaining == 0u)
            {
                break;
            }
            --remaining;
            const uint8_t value = read(source++);
            write(destination++, value);
            write(kDictionary + dictionaryWrite, value);
            dictionaryWrite = (dictionaryWrite + 1u) & 0xfffu;
            result.output.push_back(value);
            continue;
        }

        if (remaining == 0u)
        {
            break;
        }
        remaining -= 2u;
        const uint32_t first = read(source++);
        const uint32_t second = read(source++);
        const uint32_t offset = first | ((second & 0xf0u) << 4);
        const uint32_t length = (second & 0x0fu) + 3u;
        for (uint32_t index = 0u; index < length; ++index)
        {
            const uint8_t value = read(kDictionary + ((offset + index) & 0xfffu));
            write(destination++, value);
            write(kDictionary + dictionaryWrite, value);
            dictionaryWrite = (dictionaryWrite + 1u) & 0xfffu;
            result.output.push_back(value);
        }
    }
    return result;
}

std::vector<uint8_t> makeBaseMemory(const std::vector<uint8_t> &input)
{
    std::vector<uint8_t> memory(PS2_RAM_SIZE, 0u);
    if (!input.empty())
    {
        std::memcpy(memory.data() + kSource, input.data(), input.size());
    }
    std::memset(memory.data() + kDictionary, 0xa5, 0x1000u);
    std::memset(memory.data() + kDestination - 16u, 0x5a, 16u);
    return memory;
}

R5900Context makeContext()
{
    R5900Context context{};
    context.pc = 0x270f58u;
    context.r[4] = _mm_cvtsi32_si128(kSource);
    context.r[5] = _mm_cvtsi32_si128(kDestination);
    context.r[6] = _mm_cvtsi32_si128(0);
    context.r[31] = _mm_cvtsi32_si128(kReturnTarget);
    return context;
}

struct NativeResult
{
    std::vector<uint8_t> memory;
    std::vector<uint8_t> output;
    uint32_t yields = 0u;
    double elapsedMs = 0.0;
};

NativeResult runNative(PS2Runtime &runtime,
                       const std::vector<uint8_t> &base,
                       const std::vector<uint8_t> &input,
                       size_t expectedOutputBytes,
                       bool cooperative)
{
    NativeResult result{base, {}, 0u, 0.0};
    R5900Context context = makeContext();
    context.r[6] = _mm_cvtsi32_si128(static_cast<int32_t>(input.size()));
    const auto started = std::chrono::steady_clock::now();

    for (uint32_t iteration = 0u; iteration < 10000u; ++iteration)
    {
        runtime.eeScheduler().reset(result.memory.data(), context);
        if (cooperative)
        {
            runtime.eeScheduler().postEvent(EeEvent{});
        }
        if (iteration == 0u)
        {
            FUN_00270f58_0x270f58(result.memory.data(), &context, &runtime);
        }
        else
        {
            check(context.pc == 0x27103cu, "cooperative decode resumes at the control checkpoint");
            entry_0027103c_0x27103c(result.memory.data(), &context, &runtime);
        }

        if (context.pc == kReturnTarget)
        {
            break;
        }
        if (context.pc != 0x27103cu)
        {
            std::fprintf(stderr,
                         "unexpected decode stop: input=%zu cooperative=%d iteration=%u pc=0x%x\n",
                         input.size(), cooperative ? 1 : 0, iteration, context.pc);
        }
        check(context.pc == 0x27103cu,
              "decode only yields at a resumable control checkpoint");
        ++result.yields;
    }

    result.elapsedMs = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - started)
                           .count();
    if (context.pc != kReturnTarget)
    {
        std::fprintf(stderr,
                     "decode did not return: input=%zu cooperative=%d yields=%u pc=0x%x\n",
                     input.size(), cooperative ? 1 : 0, result.yields, context.pc);
    }
    check(context.pc == kReturnTarget, "decode returns to the original caller");
    const auto begin = result.memory.begin() + kDestination;
    result.output.assign(begin, begin + expectedOutputBytes);
    return result;
}

void compareWithReference(const std::string &label,
                          const ReferenceResult &expected,
                          const NativeResult &actual)
{
    if (actual.output != expected.output)
    {
        const size_t common = std::min(actual.output.size(), expected.output.size());
        size_t index = 0u;
        while (index < common && actual.output[index] == expected.output[index])
        {
            ++index;
        }
        std::fprintf(stderr,
                     "%s first output mismatch: index=%zu expected=%zu actual=%zu yields=%u\n",
                     label.c_str(), index, expected.output.size(), actual.output.size(), actual.yields);
        if (index < common)
        {
            std::fprintf(stderr, "expected byte=%02x actual byte=%02x\n",
                         expected.output[index], actual.output[index]);
        }
    }
    check(actual.output == expected.output, label + " output matches the original algorithm");
    check(std::memcmp(actual.memory.data() + kDictionary,
                      expected.memory.data() + kDictionary,
                      0x1000u) == 0,
          label + " dictionary matches the original algorithm");
    check(std::memcmp(actual.memory.data() + kDestination - 16u,
                      expected.memory.data() + kDestination - 16u,
                      16u) == 0,
          label + " preserves the destination guard");
}

void testSample(PS2Runtime &runtime,
                const std::string &label,
                const std::vector<uint8_t> &input,
                bool cooperative)
{
    const auto base = makeBaseMemory(input);
    const auto expected = decodeReference(base, input);
    const auto actual = runNative(runtime, base, input, expected.output.size(), cooperative);
    compareWithReference(label, expected, actual);
    std::printf("PASS: %s (%zu bytes, %u yields, %.3f ms)\n",
                label.c_str(), actual.output.size(), actual.yields, actual.elapsedMs);
}

void testResumeEntries(PS2Runtime &runtime)
{
    {
        auto memory = makeBaseMemory({});
        auto expected = memory;
        for (uint32_t index = 0u; index < 3u; ++index)
        {
            expected[kDictionary + 0xfedu - index] = 0x20u;
        }
        auto context = makeContext();
        context.r[2] = _mm_cvtsi32_si128(static_cast<int32_t>(kDictionary + 0xfedu));
        context.r[3] = _mm_cvtsi32_si128(0x20);
        context.r[6] = _mm_cvtsi32_si128(0);
        context.r[9] = _mm_cvtsi32_si128(2);
        runtime.eeScheduler().reset(memory.data(), context);
        entry_00270f70_0x270f70(memory.data(), &context, &runtime);
        check(context.pc == kReturnTarget, "dictionary-init resume returns");
        check(std::memcmp(memory.data() + kDictionary,
                          expected.data() + kDictionary, 0x1000u) == 0,
              "dictionary-init resume preserves the original loop boundary");
    }

    {
        auto memory = makeBaseMemory({'Z', 'Y'});
        auto expected = memory;
        expected[kDestination] = 'Z';
        expected[kDictionary + 0xfeeu] = 'Z';
        auto context = makeContext();
        context.r[6] = _mm_cvtsi32_si128(1);
        context.r[7] = _mm_cvtsi32_si128('Z');
        context.r[10] = _mm_cvtsi32_si128(0xfee);
        context.r[13] = _mm_cvtsi32_si128(static_cast<int32_t>(0xff03u));
        runtime.eeScheduler().reset(memory.data(), context);
        entry_00270fa8_0x270fa8(memory.data(), &context, &runtime);
        check(context.pc == kReturnTarget, "literal resume returns");
        auto *contextPointer = &context;
        check(GPR_U32(contextPointer, 4) == kSource + 1u &&
                  GPR_U32(contextPointer, 6) == 0u,
              "literal resume consumes exactly one declared input byte");
        check(std::memcmp(memory.data() + kDestination,
                          expected.data() + kDestination, 2u) == 0 &&
                  std::memcmp(memory.data() + kDictionary,
                              expected.data() + kDictionary, 0x1000u) == 0,
              "literal resume does not over-decode the next token");
    }

    {
        auto memory = makeBaseMemory({0xfeu, 0x00u});
        auto context = makeContext();
        context.r[6] = _mm_cvtsi32_si128(2);
        context.r[10] = _mm_cvtsi32_si128(0xfee);
        runtime.eeScheduler().reset(memory.data(), context);
        entry_00270fcc_0x270fcc(memory.data(), &context, &runtime);
        check(context.pc == kReturnTarget, "match resume returns");
        auto *contextPointer = &context;
        check(GPR_U32(contextPointer, 4) == kSource + 2u &&
                  GPR_U32(contextPointer, 6) == 0u,
              "match resume consumes the backreference pair");
        check(std::memcmp(memory.data() + kDestination, "\xa5\xa5\xa5", 3u) == 0,
              "match resume preserves the untouched dictionary tail");
    }

    {
        auto memory = makeBaseMemory({});
        memory[kDictionary + 11u] = 'R';
        memory[kDictionary + 12u] = 'S';
        auto expected = memory;
        expected[kDestination] = 'R';
        expected[kDestination + 1u] = 'S';
        expected[kDictionary + 0xfeeu] = 'R';
        expected[kDictionary + 0xfefu] = 'S';
        auto context = makeContext();
        context.r[8] = _mm_cvtsi32_si128(3);
        context.r[9] = _mm_cvtsi32_si128(10);
        context.r[10] = _mm_cvtsi32_si128(0xfee);
        context.r[11] = _mm_cvtsi32_si128(1);
        runtime.eeScheduler().reset(memory.data(), context);
        entry_00271008_0x271008(memory.data(), &context, &runtime);
        check(context.pc == kReturnTarget, "match-copy resume returns");
        check(std::memcmp(memory.data() + kDestination,
                          expected.data() + kDestination, 2u) == 0 &&
                  std::memcmp(memory.data() + kDictionary,
                              expected.data() + kDictionary, 0x1000u) == 0,
              "match-copy resume continues at the saved copy index");
    }

    {
        auto memory = makeBaseMemory({'Z'});
        auto expected = memory;
        expected[kDestination] = 'Z';
        expected[kDictionary + 0xfeeu] = 'Z';
        auto context = makeContext();
        context.r[2] = _mm_cvtsi32_si128(1);
        context.r[6] = _mm_cvtsi32_si128(1);
        context.r[10] = _mm_cvtsi32_si128(0xfee);
        context.r[13] = _mm_cvtsi32_si128(static_cast<int32_t>(0xff01u));
        runtime.eeScheduler().reset(memory.data(), context);
        entry_00271064_0x271064(memory.data(), &context, &runtime);
        check(context.pc == kReturnTarget, "literal-branch resume returns");
        check(std::memcmp(memory.data() + kDestination,
                          expected.data() + kDestination, 1u) == 0 &&
                  std::memcmp(memory.data() + kDictionary,
                              expected.data() + kDictionary, 0x1000u) == 0,
              "literal-branch resume preserves the selected token");
    }

    std::puts("PASS: all internal decoder resume entries");
}

} // namespace

int main()
{
    PS2Runtime runtime;

    StreamBuilder literals;
    for (uint8_t value = 0u; value < 8u; ++value)
    {
        literals.literal(static_cast<uint8_t>('A' + value));
    }
    testSample(runtime, "literal-only", literals.finish(), false);

    StreamBuilder initialMatch;
    initialMatch.match(0u, 3u);
    initialMatch.literal('X');
    const auto initialMatchBytes = initialMatch.finish();
    const auto initialMatchBase = makeBaseMemory(initialMatchBytes);
    const auto initialMatchResult = runNative(runtime, initialMatchBase, initialMatchBytes, 4u, false);
    check(initialMatchResult.output == std::vector<uint8_t>({0x20u, 0x20u, 0x20u, 'X'}),
          "minimum backreference and literal have the expected bytes");
    compareWithReference("minimum-match", decodeReference(initialMatchBase, initialMatchBytes),
                         initialMatchResult);
    std::puts("PASS: minimum backreference and literal");

    StreamBuilder initialTail;
    initialTail.match(0xffeu, 3u);
    const auto initialTailBytes = initialTail.finish();
    const auto initialTailBase = makeBaseMemory(initialTailBytes);
    const auto initialTailResult = runNative(runtime, initialTailBase, initialTailBytes, 3u, false);
    check(initialTailResult.output == std::vector<uint8_t>({0xa5u, 0xa5u, 0x20u}),
          "original dictionary initialization leaves its final 18 bytes untouched");
    compareWithReference("initial-dictionary-tail",
                         decodeReference(initialTailBase, initialTailBytes), initialTailResult);
    std::puts("PASS: original dictionary initialization boundary");

    StreamBuilder wrapping;
    for (uint32_t value = 0u; value < 48u; ++value)
    {
        wrapping.literal(static_cast<uint8_t>(value));
    }
    wrapping.match(0xfffu, 18u);
    testSample(runtime, "dictionary-wrap", wrapping.finish(), true);

    const std::vector<uint8_t> finalMatch{0x00u, 0x00u, 0x00u};
    const auto finalMatchBase = makeBaseMemory(finalMatch);
    const auto finalMatchResult = runNative(runtime, finalMatchBase, finalMatch, 3u, false);
    check(finalMatchResult.output == std::vector<uint8_t>({0x20u, 0x20u, 0x20u}),
          "a match can end exactly at the declared input boundary");
    compareWithReference("final-match", decodeReference(finalMatchBase, finalMatch),
                         finalMatchResult);
    std::puts("PASS: match ending at the input boundary");

    testSample(runtime, "zero-input", {}, true);
    testResumeEntries(runtime);

    StreamBuilder large;
    for (uint32_t value = 0u; value < 256u; ++value)
    {
        large.literal(static_cast<uint8_t>((value * 37u) & 0xffu));
    }
    for (uint32_t index = 0u; index < 16384u; ++index)
    {
        large.match(static_cast<uint16_t>(index & 0xffu), 18u);
    }
    const auto largeBytes = large.finish();
    testSample(runtime, "large-cooperative", largeBytes, false);
    testSample(runtime, "large-cooperative", largeBytes, true);

    return 0;
}
