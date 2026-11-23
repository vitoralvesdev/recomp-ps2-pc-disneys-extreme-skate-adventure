#include "ps2_runtime.h"
#include "runtime/ee_scheduler.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x1000u;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x1040u;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=16u;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[16] = {};
static bool visited=false;
static void yieldAtEntry(uint8_t*,R5900Context* ctx,PS2Runtime*) { visited=true; setReturnU32(ctx,0x1au); }
static void returnNormally(uint8_t*,R5900Context* ctx,PS2Runtime*) { visited=true; ctx->pc=getRegU32(ctx,31); }
static void check(bool condition,const char* label) { if(!condition){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);} }
#if defined(DISNEY_TEST_ORIGINAL_AOT)
void FUN_002d2b90_0x2d2b90(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_002d3848_0x2d3848(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_002d3888_0x2d3888(uint8_t*,R5900Context*,PS2Runtime*);
static void checkVu0Constant(const R5900Context& context,const char* label) {
 const __m128i actual=_mm_castps_si128(context.vu0_vf[0]);
 check(_mm_movemask_epi8(_mm_cmpeq_epi32(actual,_mm_set_epi32(0x3f800000,0,0,0)))==0xffff,label);
}
static void testVu0Normalization(PS2Runtime& runtime,std::vector<uint8_t>& ram) {
 struct Sample { uint32_t input[4]; uint32_t output[4]; };
 const Sample samples[] = {
  {{0xc4e130fcu,0xc11baa70u,0xc2aca27au,0x3f800000u},{0x80000000u,0x80000000u,0x80000000u,0u}},
  {{0xc3a46830u,0xbfb6fe40u,0x4240b8bau,0x3f800000u},{0x80000000u,0x80000000u,0u,0u}},
 };
 for(const auto& sample:samples) {
  const auto& input=sample.input;
  const auto normalize=[&](R5900Context& context) {
   std::memcpy(ram.data()+0x1000,input,sizeof(input));
   context.r[4]=_mm_cvtsi32_si128(0x1000); context.r[5]=_mm_cvtsi32_si128(0x1000); context.r[31]=_mm_cvtsi32_si128(0x2000);
   FUN_002d2b90_0x2d2b90(ram.data(),&context,&runtime);
  };
  R5900Context broken;
  broken.vu0_vf[0]=_mm_setzero_ps();
  normalize(broken);
  float out[4];
  std::memcpy(out,ram.data()+0x1000,sizeof(out));
  check(broken.vu0_q==0.0f && out[0]==0.0f && out[1]==0.0f && out[2]==0.0f,"zero VF0 reproduces recorded failure");
  check(std::memcmp(ram.data()+0x1000,sample.output,sizeof(sample.output))==0,"zero VF0 reproduces recorded output bits");
  R5900Context context;
  checkVu0Constant(context,"fresh context initializes constant VF0");
  normalize(context);
  float in[4];
  std::memcpy(in,input,sizeof(in)); std::memcpy(out,ram.data()+0x1000,sizeof(out));
  check(context.vu0_q>0.0f,"fresh context normalizes recorded nonzero vector");
  const float length=std::sqrt(in[0]*in[0]+in[1]*in[1]+in[2]*in[2]);
  for(int i=0;i<3;++i) check(std::fabs(out[i]-in[i]/length)<0.000001f,"recorded normalized component");
  check(out[3]==0.0f,"normalized w");
 }
 std::puts("PASS: VU0 normalization of recorded inputs in fresh contexts");
}
static void testVu0DataMemory(PS2Runtime& runtime,std::vector<uint8_t>& ram) {
 check(runtime.memory().initialize(ram.size()),"VU0 data-memory test initialized");
 uint8_t* vu0=runtime.memory().getVU0Data();
 check(vu0!=nullptr,"VU0 data memory is allocated");
 std::memset(vu0,0,PS2_VU0_DATA_SIZE);
 const uint32_t sentinel=0x11223344u;
 std::memcpy(ram.data()+0x200,&sentinel,sizeof(sentinel));
 R5900Context ctx;
 ctx.r[29]=_mm_cvtsi32_si128(0x4000);
 ctx.r[31]=_mm_cvtsi32_si128(0x2000);
 const float first[4]={1.0f,2.0f,3.0f,4.0f};
 std::memcpy(ram.data()+0x3000,first,sizeof(first));
 ctx.r[4]=_mm_cvtsi32_si128(0x20);        // qword index 0x20 -> byte 0x200
 ctx.r[5]=_mm_cvtsi32_si128(0x3000);      // source vector
 FUN_002d3848_0x2d3848(ram.data(),&ctx,&runtime);
 const float* stored=reinterpret_cast<const float*>(vu0+0x200);
 for(int i=0;i<4;++i) check(stored[i]==first[i],"VSQI stores to VU0 data memory");
 check(std::memcmp(ram.data()+0x200,&sentinel,sizeof(sentinel))==0,
       "VSQI does not write EE RDRAM at the VU0 offset");
 const float second[4]={-5.0f,6.0f,-7.0f,8.0f};
 std::memcpy(vu0+0x200,second,sizeof(second));
 ctx.r[4]=_mm_cvtsi32_si128(0x3100);      // destination vector
 ctx.r[5]=_mm_cvtsi32_si128(0x20);        // qword index
 FUN_002d3888_0x2d3888(ram.data(),&ctx,&runtime);
 float loaded[4];
 std::memcpy(loaded,ram.data()+0x3100,sizeof(loaded));
 for(int i=0;i<4;++i) check(loaded[i]==second[i],"VLQI reads VU0 data memory");
 // VU0 data memory is 4 KB; byte 0x1200 wraps to 0x200.
 const float wrapped[4]={9.0f,10.0f,11.0f,12.0f};
 std::memcpy(ram.data()+0x3200,wrapped,sizeof(wrapped));
 ctx.r[4]=_mm_cvtsi32_si128(0x120);
 ctx.r[5]=_mm_cvtsi32_si128(0x3200);
 FUN_002d3848_0x2d3848(ram.data(),&ctx,&runtime);
 const float* wrappedStored=reinterpret_cast<const float*>(vu0+0x200);
 for(int i=0;i<4;++i) check(wrappedStored[i]==wrapped[i],"VSQI address wraps inside VU0 data memory");
 std::puts("PASS: EE VLQI/VSQI exchange quadwords through VU0 data memory");
}
#endif
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
#if defined(DISNEY_TEST_ORIGINAL_AOT)
 testVu0Normalization(runtime,ram);
#endif
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
#if defined(DISNEY_TEST_ORIGINAL_AOT)
 testVu0DataMemory(runtime,ram);
#endif
}





