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
static void sparseHandler(uint8_t*,R5900Context* ctx,PS2Runtime*) { ctx->pc=getRegU32(ctx,31); }
static void check(bool condition,const char* label) { if(!condition){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);} }
#if defined(DISNEY_TEST_ORIGINAL_AOT)
void entry_303910_0x303948(uint8_t*,R5900Context*,PS2Runtime*);
void entry_3040b8_0x3040f0(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_003040f0_0x3040f0(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_00304180_0x304180(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_003041c0_0x3041c0(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_00304210_0x304210(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_00304260_0x304260(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_00303f90_0x303f90(uint8_t*,R5900Context*,PS2Runtime*);
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
static void testOriginalSyscallLookup(PS2Runtime& runtime,std::vector<uint8_t>& ram) {
 const uint32_t handlers[][2]={
  {0x000000fcu,0x80076440u},{0x000000feu,0x80076440u},
  {0x000000fdu,0x800762a0u},{0x000000ffu,0x800762a0u},
  {0x0000012cu,0x80076488u},{0x00000008u,0x800766c0u},
 };
 std::memcpy(ram.data()+0x76710u,handlers,sizeof(handlers));
 for(const auto& handler:handlers) {
  R5900Context ctx;
  ctx.pc=0x303910u;
  ctx.r[4]=_mm_cvtsi32_si128(handler[0]);
  ctx.r[31]=_mm_cvtsi32_si128(0x2000u);
  entry_303910_0x303948(ram.data(),&ctx,&runtime);
  check(getRegU32(&ctx,2)==handler[1] && ctx.pc==0x2000u,
       "original GetEntryAddress returns the copied handler");
 }
 R5900Context unknown;
 unknown.pc=0x303910u;
 unknown.r[4]=_mm_cvtsi32_si128(0x123u);
 unknown.r[31]=_mm_cvtsi32_si128(0x2000u);
 entry_303910_0x303948(ram.data(),&unknown,&runtime);
 check(getRegU32(&unknown,2)==0u && unknown.pc==0x2000u,
       "original GetEntryAddress returns zero for an unknown syscall");
 std::puts("PASS: original GetEntryAddress resolves all six copied syscall handlers");
}
static void testOriginalCopiedSyscallLookup(PS2Runtime& runtime,std::vector<uint8_t>& ram) {
 const uint32_t handlers[][2]={
  {0x55u,0x80075038u},{0x56u,0x800750c8u},{0x57u,0x80075108u},
  {0x58u,0x80075158u},{0x59u,0x800751a8u},{0x03u,0x80075330u},
 };
 std::memcpy(ram.data()+0x75300u,handlers,sizeof(handlers));
 for(const auto& handler:handlers) {
  R5900Context ctx;
  ctx.pc=0x3040b8u;
  ctx.r[4]=_mm_cvtsi32_si128(handler[0]);
  ctx.r[31]=_mm_cvtsi32_si128(0x2000u);
  entry_3040b8_0x3040f0(ram.data(),&ctx,&runtime);
  check(getRegU32(&ctx,2)==handler[1] && ctx.pc==0x2000u,
       "original copied syscall lookup returns its handler");
 }
 R5900Context unknown;
 unknown.pc=0x3040b8u;
 unknown.r[4]=_mm_cvtsi32_si128(0x5bu);
 unknown.r[31]=_mm_cvtsi32_si128(0x2000u);
 entry_3040b8_0x3040f0(ram.data(),&unknown,&runtime);
 check(getRegU32(&unknown,2)==0u && unknown.pc==0x2000u,
       "original copied syscall lookup returns zero for an unknown syscall");
 std::puts("PASS: original copied syscall lookup resolves all six table entries");
}
static void testOriginalCopiedSyscallDispatch(PS2Runtime& runtime,std::vector<uint8_t>& ram) {
 check(runtime.memory().initialize(ram.size()),"copied-handler dispatch test initialized");
 const auto registerRelocated=[&runtime](uint32_t sourceBegin,uint32_t sourceEnd,
                                         uint32_t destination,PS2Runtime::RecompiledFunction function) {
  runtime.registerFunctionRange(sourceBegin,sourceEnd,function);
  runtime.registerFunctionRange(destination,destination+sourceEnd-sourceBegin,function);
  runtime.registerFunctionRange(destination|0x80000000u,(destination|0x80000000u)+sourceEnd-sourceBegin,function);
  runtime.registerFunction(destination,function);
  runtime.registerFunction(destination|0x80000000u,function);
 };
 registerRelocated(0x3040b8u,0x3040f0u,0x75000u,&entry_3040b8_0x3040f0);
 registerRelocated(0x3040f0u,0x304180u,0x75038u,&FUN_003040f0_0x3040f0);
 registerRelocated(0x304180u,0x3041c0u,0x750c8u,&FUN_00304180_0x304180);
 registerRelocated(0x3041c0u,0x304210u,0x75108u,&FUN_003041c0_0x3041c0);
 registerRelocated(0x304210u,0x304260u,0x75158u,&FUN_00304210_0x304210);
 registerRelocated(0x304260u,0x304390u,0x751a8u,&FUN_00304260_0x304260);
 const uint32_t handlers[][2]={
  {0x80075038u,0x3040f0u},{0x800750c8u,0x304180u},{0x80075108u,0x3041c0u},
  {0x75158u,0x304210u},{0x800751a8u,0x304260u},{0x80075010u,0x3040c8u},
 };
 for(const auto& handler:handlers) {
  check(runtime.hasFunction(handler[0]) && runtime.hasFunction(handler[1]),
       "copied handler and source alias resolve");
 }
 const uint32_t resumeHandler[][2]={{0x55u,0x80075010u}};
 std::memcpy(ram.data()+0x75300u,resumeHandler,sizeof(resumeHandler));
 R5900Context lookupResume;
 lookupResume.pc=0x3040c8u;
 lookupResume.r[3]=_mm_cvtsi32_si128(0x80075300u);
 lookupResume.r[4]=_mm_cvtsi32_si128(0x55u);
 lookupResume.r[5]=_mm_cvtsi32_si128(0u);
 lookupResume.r[31]=_mm_cvtsi32_si128(0x2000u);
 check(runtime.dispatchGuestBranch(ram.data(),&lookupResume,0x3040c8u,0x3040c8u,0x2000u,
                                  PS2Runtime::GuestBranchKind::DirectCall,"copied-lookup-resume"),
       "copied lookup resumes at its internal loop entry");
 check(getRegU32(&lookupResume,2)==0x80075010u && lookupResume.pc==0x2000u,
       "copied lookup internal entry maps to 0x80075010");
 struct DispatchSample { uint32_t target; int argumentRegister; uint32_t argument; uint32_t result; };
 const DispatchSample samples[]={
  {0x80075038u,5,0xff000000u,0xffffffffu},{0x800750c8u,4,0x30u,0xffffffffu},
  {0x80075108u,4,0x30u,0xffffffffu},{0x75158u,4,0u,0xffffffffu},{0x800751a8u,4,1u,0xffffffffu},
 };
 for(const auto& sample:samples) {
  R5900Context context;
  context.pc=sample.target;
  context.r[sample.argumentRegister]=_mm_cvtsi32_si128(sample.argument);
  context.r[29]=_mm_cvtsi32_si128(0x10000u);
  context.r[31]=_mm_cvtsi32_si128(0x2000u);
  const bool completed=runtime.dispatchGuestBranch(ram.data(),&context,sample.target,0x900u,0x2000u,
                                                  PS2Runtime::GuestBranchKind::DirectCall,"copied-handler-dispatch");
  if(!completed) std::fprintf(stderr,"copied handler target=0x%x did not complete; pc=0x%x\n",sample.target,context.pc);
  check(completed,"copied handler dispatches through its registered alias");
  if(getRegU32(&context,2)!=sample.result || context.pc!=0x2000u) {
   std::fprintf(stderr,"copied handler target=0x%x expected=0x%x actual=0x%x pc=0x%x\n",
                sample.target,sample.result,getRegU32(&context,2),context.pc);
  }
  check(getRegU32(&context,2)==sample.result && context.pc==0x2000u,
       "copied handler returns its original result");
 }
 R5900Context nested;
 nested.pc=0x800751a8u;
 nested.r[4]=_mm_cvtsi32_si128(0x70000000u);
 nested.r[29]=_mm_cvtsi32_si128(0x10000u);
 nested.r[31]=_mm_cvtsi32_si128(0x2000u);
 check(runtime.dispatchGuestBranch(ram.data(),&nested,0x800751a8u,0x900u,0x2000u,
                                  PS2Runtime::GuestBranchKind::DirectCall,"copied-handler-nested-call"),
       "copied TLB routine completes its nested 0x75158 call");
 check(getRegU32(&nested,2)==0u && nested.pc==0x2000u,
       "copied TLB routine resumes after the nested 0x3042ac continuation");
 std::puts("PASS: copied syscall handlers dispatch physical/KSEG0 aliases and resume internal entries");
}
static void testOriginalExceptionReturnEntry(PS2Runtime& runtime,std::vector<uint8_t>& ram) {
 const uint32_t savedRa=0x11223344u;
 const uint32_t savedSp=0x55667788u;
 std::memcpy(ram.data()+0x76c40u,&savedRa,sizeof(savedRa));
 std::memcpy(ram.data()+0x76c50u,&savedSp,sizeof(savedSp));
 R5900Context ctx;
 ctx.pc=0x303fc0u;
 ctx.cop0_epc=0x2000u;
 ctx.r[26]=_mm_cvtsi32_si128(0x12u);
 FUN_00303f90_0x303f90(ram.data(),&ctx,&runtime);
 check(ctx.pc==0x2000u,
       "original exception-return internal entry resumes EPC");
 check(ctx.cop0_status==0x10u,
       "original exception-return internal entry clears EXL");
 uint32_t stored=0;
 std::memcpy(&stored,ram.data()+0x76c40u,sizeof(stored));
 check(stored==savedRa,"internal exception-return entry does not overwrite saved RA");
 std::memcpy(&stored,ram.data()+0x76c50u,sizeof(stored));
 check(stored==savedSp,"internal exception-return entry does not overwrite saved SP");
 std::puts("PASS: original exception-return internal entry skips the copied prologue");
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
#if defined(DISNEY_TEST_ORIGINAL_AOT)
static void testInvalidDmaAddresses() {
 PS2Memory memory;
 check(memory.initialize(),"invalid-DMA test initialized");
 GifArbiter sink([](const uint8_t*,uint32_t){});
 memory.setGifArbiter(&sink);
 memory.write32(0x1000e000u,1u);
 for(const uint32_t channel:{0x10008000u,0x10009000u,0x1000a000u}) {
  const uint32_t expected=(channel-0x10008000u)/0x1000u;
  memory.write32(channel+0x10u,0x10000000u);
  memory.write32(channel+0x20u,1u);
  memory.write32(channel,0x101u);
  check(memory.consumeCompletedDmacCauses().empty(),"invalid direct DMA source does not complete");
  check(memory.read32(channel+0x20u)==1u,"invalid direct DMA preserves QWC");
  check((memory.read32(0x1000e010u)&(1u<<expected))==0u,"invalid direct DMA does not raise D_STAT");

  memory.write32(channel+0x20u,0u);
  memory.write32(channel+0x30u,0x10000000u);
  memory.write32(channel,0x105u);
  check(memory.consumeCompletedDmacCauses().empty(),"invalid chain tag does not complete");
  check((memory.read32(0x1000e010u)&(1u<<expected))==0u,"invalid chain tag does not raise D_STAT");

  memory.write32(0x2000u,1u);
  memory.write32(0x2004u,0x10000000u);
  memory.write32(channel+0x30u,0x2000u);
  memory.write32(channel,0x105u);
  check(memory.consumeCompletedDmacCauses().empty(),"invalid chain payload does not complete");
  check((memory.read32(0x1000e010u)&(1u<<expected))==0u,"invalid chain payload does not raise D_STAT");

  memory.write32(0x2000u,0x70000000u);
  memory.write32(channel+0x30u,0x2000u);
  memory.write32(channel,0x105u);
  const auto causes=memory.consumeCompletedDmacCauses();
  check(causes.size()==1u && causes.front()==expected,"zero-payload END chain still completes");
  check((memory.read32(0x1000e010u)&(1u<<expected))!=0u,"valid END chain raises D_STAT");
 }
 std::puts("PASS: invalid GIF/VIF DMA addresses suppress completion while empty END chains complete");
}
#endif
#include "test_native_vblank.inl"
#include "test_irq_atomic.inl"
int main() {
 PS2Runtime runtime; R5900Context context;
#if defined(DISNEY_TEST_ORIGINAL_AOT)
 check(runtime.missingFunctionPolicy()==PS2Runtime::MissingFunctionPolicy::Stop,"missing EE functions stop by default");
#endif
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
 runtime.registerFunction(0x76000u,&sparseHandler);
 runtime.registerFunction(0x80076000u,&sparseHandler);
 check(runtime.hasFunction(0x76000u) && runtime.hasFunction(0x80076000u),"sparse registrations are visible outside the dense table");
 context.pc=0x900u;
 const bool sparseCompleted=runtime.dispatchGuestBranch(nullptr,&context,0x80076000u,0x900u,0x2000u,PS2Runtime::GuestBranchKind::DirectCall,"sparse-regression");
 check(sparseCompleted && context.pc==0x2000u,"sparse function dispatches and returns normally");
 runtime.registerFunctionRange(0x303910u,0x303948u,&sparseHandler);
 check(runtime.hasFunction(0x303920u),"sparse ranges expose internal resume points");
 context.pc=0x303920u;
 const bool resumeCompleted=runtime.dispatchGuestBranch(nullptr,&context,0x303920u,0x303920u,0x2000u,PS2Runtime::GuestBranchKind::DirectCall,"sparse-resume");
 check(resumeCompleted && context.pc==0x2000u,"sparse ranges dispatch cooperative resume points");
 runtime.registerFunctionRange(0x76680u,0x766ccu,&returnNormally);
 runtime.registerFunction(0x766c0u,&sparseHandler);
 runtime.registerFunction(0x800766c0u,&sparseHandler);
 check(runtime.lookupFunction(0x766c0u)==&sparseHandler &&
       runtime.lookupFunction(0x800766c0u)==&sparseHandler,
       "exact relocated internal entries override their containing ranges");
 std::puts("PASS: relocated handlers and internal resume points resolve outside the dense AOT table");
 runtime.registerFunction(0x1018u,&recursiveGuest);
 const bool recursiveCompleted=runtime.dispatchGuestBranch(nullptr,&context,0x1018u,0x900u,0x1020u,PS2Runtime::GuestBranchKind::DirectCall,"depth-test");
 check(!recursiveCompleted && context.pc==0x1018u,"native depth limit preserves guest target");
 check(recursiveCalls>0u && recursiveCalls<=64u,"native calls have bounded depth");
 std::puts("PASS: deep guest calls yield before native stack overflow");
 std::vector<uint8_t> ram(0x2000000u);
#if defined(DISNEY_TEST_ORIGINAL_AOT)
 testOriginalSyscallLookup(runtime,ram);
 testOriginalCopiedSyscallLookup(runtime,ram);
 testOriginalCopiedSyscallDispatch(runtime,ram);
 testOriginalExceptionReturnEntry(runtime,ram);
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
 testInvalidDmaAddresses();
 testVu0DataMemory(runtime,ram);
#endif
 auto fatalRuntime=std::make_unique<PS2Runtime>(); R5900Context fatalContext;
 fatalRuntime->setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
 fatalContext.r[31]=_mm_cvtsi32_si128(0x2000);
 (void)fatalRuntime->dispatchGuestBranch(nullptr,&fatalContext,0x1038u,0x900u,0x2000u,PS2Runtime::GuestBranchKind::DirectCall,"missing-function-fatal");
 check(fatalRuntime->hadFatalError() && fatalRuntime->isStopRequested(),"fatal missing functions stop and remain distinguishable from normal exits");
 std::puts("PASS: fatal missing functions produce a fatal runtime state");
}





