#include <chrono>
#include <thread>
static unsigned nativeDisplayInterrupts=0;
static std::chrono::steady_clock::time_point nativeDisplayStart;
static void tinyCycleWork(uint8_t*,R5900Context* ctx,PS2Runtime* runtime) {
    // Deliberately tiny virtual progress, real host work: reproduces the
    // renderer/checkpoint clock mismatch without booting the game.
    runtime->eeScheduler().accountCycles(1u);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ctx->pc=0x1024u;
    if(std::chrono::steady_clock::now()-nativeDisplayStart>=std::chrono::milliseconds(180))runtime->requestStop();
}
static void nativeDisplayIrq(uint8_t*,R5900Context* ctx,PS2Runtime*) {
    ++nativeDisplayInterrupts;ctx->pc=getRegU32(ctx,31);
}
static void testNativeDisplayClock(PS2Runtime& runtime,std::vector<uint8_t>& ram) {
    R5900Context first;first.pc=0x1024u;
    runtime.registerFunction(0x1024u,&tinyCycleWork);runtime.registerFunction(0x1028u,&nativeDisplayIrq);
    runtime.eeScheduler().reset(ram.data(),first);
    runtime.eeScheduler().addIrqHandler(false,2u,0x1028u,false,0u,0u,0u);
    nativeDisplayStart=std::chrono::steady_clock::now();runtime.eeScheduler().run();
    const auto ticks=runtime.memory().gs().vsyncTick.load();
    const auto cycles=runtime.eeScheduler().snapshot().eeCycle;
    check(cycles<100000u,"display cadence does not manufacture EE/DMA/IOP work cycles");
    const char* nativeVblank=std::getenv("DISNEY_NATIVE_VBLANK");
    if(!nativeVblank || nativeVblank[0]!='0') {
        check(ticks>=8u && ticks<=16u,"native VBlank cadence while EE cycles lag");
        check(nativeDisplayInterrupts>=8u && nativeDisplayInterrupts<=ticks,"original VBlank IRQs remain dispatched");
    } else check(ticks==0u && nativeDisplayInterrupts==0u,"reference demonstrates cycle-gated display starvation");
    std::printf("PASS: display-clock regression ticks=%llu callbacks=%u deviceCycles=%llu; no game launched\n",static_cast<unsigned long long>(ticks),nativeDisplayInterrupts,static_cast<unsigned long long>(cycles));
}
