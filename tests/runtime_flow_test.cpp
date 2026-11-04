#include "ps2_runtime.h"
#include "runtime/ee_scheduler.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x1000u;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x1040u;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=16u;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[16] = {};
static bool visited=false;
static void yieldAtEntry(uint8_t*,R5900Context* ctx,PS2Runtime*) { visited=true; setReturnU32(ctx,0x1au); }
static void returnNormally(uint8_t*,R5900Context* ctx,PS2Runtime*) { visited=true; ctx->pc=getRegU32(ctx,31); }
static void check(bool condition,const char* label) { if(!condition){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);} }
static unsigned gsInterrupts=0;
static void signalMasked(uint8_t*,R5900Context* ctx,PS2Runtime* runtime) {
 runtime->memory().gs().imr=0x1f00u;
 runtime->memory().gs().csr.fetch_or(1u);
 ctx->pc=0x100cu;
}
static void unmaskGs(uint8_t*,R5900Context* ctx,PS2Runtime* runtime) {
 check(gsInterrupts==0,"masked GS signal is not delivered");
 runtime->memory().gs().imr &= ~0x100ull;
 ctx->pc=0x1010u;
}
static void acknowledgeGs(uint8_t*,R5900Context* ctx,PS2Runtime* runtime) {
 ++gsInterrupts;
 runtime->memory().gs().csr.fetch_and(~1ull);
 ctx->pc=getRegU32(ctx,31);
}
static void checkGsDelivery(uint8_t*,R5900Context*,PS2Runtime* runtime) {
 check(gsInterrupts==1,"unmasked GS signal delivered once before main resumes");
 runtime->requestStop();
}
static unsigned recursiveCalls=0;
static void recursiveGuest(uint8_t* ram,R5900Context* ctx,PS2Runtime* runtime) {
 ++recursiveCalls;
 (void)runtime->dispatchGuestBranch(ram,ctx,0x1018u,0x1018u,0x1020u,PS2Runtime::GuestBranchKind::DirectCall,"recursive-test");
}
#include "test_native_vblank.inl"
#include "test_irq_atomic.inl"
int main() {
 PS2Runtime runtime; R5900Context context;
 const uint32_t callbackTop=runtime.reserveAsyncCallbackStack(0x4000,16);
 const uint32_t secondTop=runtime.reserveAsyncCallbackStack(0x4000,16);
 check(callbackTop>=0xc4000u && callbackTop<0x100000u,"callback excludes game heaps and main stack");
 check(secondTop!=0 && secondTop+0x4000u<=callbackTop,"callback stacks do not overlap");
 runtime.registerFunction(0x1000,&yieldAtEntry); runtime.registerFunction(0x1004,&returnNormally);
 context.r[31]=_mm_cvtsi32_si128(0x2000);
 bool completed=runtime.dispatchGuestBranch(nullptr,&context,0x1000,0x900,0x2000,PS2Runtime::GuestBranchKind::DirectCall,"regression-yield");
 check(visited,"callee executed"); check(!completed,"yield is not a completed call"); check(context.pc==0x1000,"yield preserves guest PC");
 visited=false;
 completed=runtime.dispatchGuestBranch(nullptr,&context,0x1004,0x900,0x2000,PS2Runtime::GuestBranchKind::DirectCall,"regression-return");
 check(visited && completed && context.pc==0x2000,"normal return resumes caller");
 std::puts("PASS: cooperative yield and normal return remain distinct");
 runtime.registerFunction(0x1018u,&recursiveGuest);
 const bool recursiveCompleted=runtime.dispatchGuestBranch(nullptr,&context,0x1018u,0x900u,0x1020u,PS2Runtime::GuestBranchKind::DirectCall,"depth-test");
 check(!recursiveCompleted && context.pc==0x1018u,"native depth limit preserves guest target");
 check(recursiveCalls>0u && recursiveCalls<=64u,"native calls have bounded depth");
 std::puts("PASS: deep guest calls yield before native stack overflow");
 std::vector<uint8_t> ram(0x2000000u);
 R5900Context initial;
 initial.pc=0x1008u;
 runtime.registerFunction(0x1008u,&signalMasked);
 runtime.registerFunction(0x100cu,&unmaskGs);
 runtime.registerFunction(0x1010u,&checkGsDelivery);
 runtime.registerFunction(0x1014u,&acknowledgeGs);
 runtime.eeScheduler().reset(ram.data(),initial);
 runtime.eeScheduler().addIrqHandler(false,0u,0x1014u,false,0u,0u,0u);
 runtime.eeScheduler().run();
 std::puts("PASS: GS interrupt masking, delivery and acknowledgement");
 testNativeDisplayClock(runtime,ram);
 testInterruptAtomicity(runtime,ram);
}





