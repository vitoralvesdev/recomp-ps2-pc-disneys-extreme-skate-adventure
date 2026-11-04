#include "ps2_runtime.h"
#include "ps2_recompiled_functions.h"
#include "hang_diagnostics.h"
#include <cstdio>
#include <cstdlib>
namespace {
void sampleCursor(uint8_t* ram,R5900Context* ctx) {
 static uint64_t calls=0; static unsigned written=0;
 if ((calls++ % 262144u) != 0 || written++ >= 24) return;
 const uint32_t cursor=getRegU32(ctx,17);
 std::fprintf(stderr,"[script-cursor] pc=%08x s0=%08x s1=%08x s2=%08x s3=%08x s4=%08x bytes=",ctx->pc,getRegU32(ctx,16),cursor,getRegU32(ctx,18),getRegU32(ctx,19),getRegU32(ctx,20));
 if (cursor < 0x1fffff0u) for(unsigned i=0;i<8;++i) std::fprintf(stderr,"%02x",ram[cursor+i]);
 std::fprintf(stderr,"\n");
}
void cursor388(uint8_t* ram,R5900Context* ctx,PS2Runtime* rt) { sampleCursor(ram,ctx); entry_00134388_0x134388(ram,ctx,rt); }
void cursor510(uint8_t* ram,R5900Context* ctx,PS2Runtime* rt) { sampleCursor(ram,ctx); entry_00134510_0x134510(ram,ctx,rt); }
void cursor524(uint8_t* ram,R5900Context* ctx,PS2Runtime* rt) { sampleCursor(ram,ctx); entry_00134524_0x134524(ram,ctx,rt); }
}
namespace {
void probePointer(uint8_t* ram,R5900Context* ctx,PS2Runtime* rt,PS2Runtime::RecompiledFunction function,const char* name) {
 const uint32_t originalEntry=ctx->pc;
 const uint32_t input=getRegU32(ctx,4), third=getRegU32(ctx,6), saved=getRegU32(ctx,17), caller=getRegU32(ctx,31);
 function(ram,ctx,rt);
 static unsigned yieldReports=0;
 if(ctx->pc==originalEntry && originalEntry!=caller && yieldReports++<16) std::fprintf(stderr,"[script-yield-at-entry] fn=%s entry=%08x caller=%08x in=%08x out=%08x\n",name,originalEntry,caller,input,getRegU32(ctx,2));
 static unsigned reports=0;
 if(reports<24 && ctx->pc==caller && ((input>=0x100000u && getRegU32(ctx,2)<0x100000u) || saved!=getRegU32(ctx,17))) {
  ++reports; std::fprintf(stderr,"[script-return] fn=%s caller=%08x in=%08x third=%08x out=%08x s1before=%08x s1after=%08x\n",name,caller,input,third,getRegU32(ctx,2),saved,getRegU32(ctx,17));
 }
}
void nextPointer(uint8_t* ram,R5900Context* ctx,PS2Runtime* rt) { probePointer(ram,ctx,rt,&FUN_001325f8_0x1325f8,"next"); }
void skipPointer(uint8_t* ram,R5900Context* ctx,PS2Runtime* rt) { probePointer(ram,ctx,rt,&FUN_00132940_0x132940,"skip"); }
void parsePointer(uint8_t* ram,R5900Context* ctx,PS2Runtime* rt) { probePointer(ram,ctx,rt,&FUN_00133e38_0x133e38,"value"); }
}namespace {
void sampleGeometryCopy(uint8_t* ram,R5900Context* ctx,PS2Runtime* rt) {
 static uint64_t count=0;
 if((count++ % 1048576u)==0) {
  const uint32_t sp=getRegU32(ctx,29);
  uint32_t bound=0;
  if(sp<0x1ffff70u) std::memcpy(&bound,ram+sp+128,4);
  static bool dumped=false;
  if(!dumped && bound>65536u) {
   dumped=true;
   const uint32_t start=getRegU32(ctx,30)-4u*getRegU32(ctx,3);
   std::fprintf(stderr,"[geometry-invalid] payload=%08x s0=%08x s7=%08x header=",start,getRegU32(ctx,16),getRegU32(ctx,23));
   if(start>=64 && start<0x1ffffc0u) for(unsigned i=0;i<32;++i) {uint32_t value=0;std::memcpy(&value,ram+start-64+4*i,4);std::fprintf(stderr,"%08x,",value);}
   std::fprintf(stderr,"\n");
   if(FILE* file=std::fopen("analysis/geometry-blocked.ram","wb")){std::fwrite(ram,1,0x2000000,file);std::fclose(file);}
  }
  std::fprintf(stderr,"[geometry-copy] i=%08x bound=%08x src=%08x dst=%08x sp=%08x\n",getRegU32(ctx,3),bound,getRegU32(ctx,30),getRegU32(ctx,8),sp);
 }
 entry_0015d950_0x15d950(ram,ctx,rt);
}
}namespace {
void sampleGeometryEntry(uint8_t* ram,R5900Context* ctx,PS2Runtime* rt) {
 static unsigned reports=0;
 if(reports++<32) {
  const uint32_t source=getRegU32(ctx,4), sp=getRegU32(ctx,29);
  uint32_t version=0; std::memcpy(&version,ram+0x346bc0,4);
  std::fprintf(stderr,"[geometry-entry] src=%08x object=%08x a2=%08x a3=%08x t0=%08x t1=%08x sp=%08x caller=%08x version=%u words=",source,getRegU32(ctx,5),getRegU32(ctx,6),getRegU32(ctx,7),getRegU32(ctx,8),getRegU32(ctx,9),sp,getRegU32(ctx,31),version);
  if(source<0x1ffff80u) for(unsigned i=0;i<24;++i) { uint32_t value=0; std::memcpy(&value,ram+source+4*i,4); std::fprintf(stderr,"%08x,",value); }
  std::fprintf(stderr,"\n");
 }
 FUN_0015d698_0x15d698(ram,ctx,rt);
}
}
namespace {
void sampleRenderWait(uint8_t* ram,R5900Context* ctx,PS2Runtime* rt) {
 FUN_0015ca40_0x15ca40(ram,ctx,rt);
 const uint32_t busy=getRegU32(ctx,2);
 static uint32_t last=0; static unsigned reports=0;
 if(busy!=0 && busy!=last && reports++<8){
  last=busy;
  std::fprintf(stderr,"[render-wait] node=%08x vu_pc=%x vu_cycles=%llu registers=",busy,rt->vu1().state().pc,static_cast<unsigned long long>(rt->vu1().state().cycles));
  for(uint32_t address:{0x10009000u,0x10009010u,0x10009020u,0x10009030u,0x1000a000u,0x1000a010u,0x1000a020u,0x1000a030u,0x10003c00u,0x10003ca0u,0x10003020u,0x1000e010u})std::fprintf(stderr,"%08x:%08x,",address,rt->memory().read32(address));
  std::fprintf(stderr," nodeWords=");
  if(busy<0x1ffff80u)for(unsigned i=0;i<24;++i){uint32_t word=0;std::memcpy(&word,ram+busy+4*i,4);std::fprintf(stderr,"%08x,",word);}
  std::fprintf(stderr,"\n");
  if(FILE* file=std::fopen("analysis/render-wait.ram","wb")){std::fwrite(ram,1,0x2000000,file);std::fclose(file);}
  if(FILE* file=std::fopen("analysis/render-vu1-code.bin","wb")){std::fwrite(rt->memory().getVU1Code(),1,0x4000,file);std::fclose(file);}
  if(FILE* file=std::fopen("analysis/render-vu1-data.bin","wb")){std::fwrite(rt->memory().getVU1Data(),1,0x4000,file);std::fclose(file);}
 }
}
}

namespace {
template<uint32_t Address> PS2Runtime::RecompiledFunction originalQueueBlock=nullptr;
uint64_t queueProbeFlags(uint8_t* ram){uint32_t a=0,b=0;std::memcpy(&a,ram+0x362610,4);std::memcpy(&b,ram+0x362614,4);return a|((uint64_t)b<<32);}
template<uint32_t Address> void queueProbe(uint8_t* ram,R5900Context* ctx,PS2Runtime* rt){
 DisneyHang::gsEvent(4,Address,queueProbeFlags(ram));
 originalQueueBlock<Address>(ram,ctx,rt);
 DisneyHang::gsEvent(5,Address,queueProbeFlags(ram));
}
template<uint32_t Address> void installQueueProbe(PS2Runtime& rt){
 originalQueueBlock<Address> =rt.lookupFunction(Address);rt.replaceFunction(Address,&queueProbe<Address>);
}
}

void registerDisneyDiagnostics(PS2Runtime& rt) {
 if(DisneyHang::enabled()){
  installQueueProbe<0x15a4c0u>(rt);
  installQueueProbe<0x15a518u>(rt);
  installQueueProbe<0x15a548u>(rt);
  installQueueProbe<0x15a5d0u>(rt);
  installQueueProbe<0x15a610u>(rt);
  installQueueProbe<0x15a614u>(rt);
  installQueueProbe<0x15a624u>(rt);
  installQueueProbe<0x15a67cu>(rt);
  installQueueProbe<0x15a690u>(rt);
  installQueueProbe<0x15a730u>(rt);
  installQueueProbe<0x15a760u>(rt);
  installQueueProbe<0x15a7d4u>(rt);
  installQueueProbe<0x15a844u>(rt);
  installQueueProbe<0x15a880u>(rt);
  installQueueProbe<0x15a89cu>(rt);
  installQueueProbe<0x15a8e4u>(rt);
  installQueueProbe<0x15a8e8u>(rt);
  installQueueProbe<0x166928u>(rt);
 }

 if (!std::getenv("DISNEY_PORT_DIAGNOSTICS")) return;
 rt.replaceFunction(0x15ca40,&sampleRenderWait); rt.replaceFunction(0x15d950,&sampleGeometryCopy); rt.replaceFunction(0x15d698,&sampleGeometryEntry);
 rt.replaceFunction(0x1325f8,&nextPointer); rt.replaceFunction(0x132940,&skipPointer); rt.replaceFunction(0x133e38,&parsePointer);
 rt.replaceFunction(0x134388,&cursor388); rt.replaceFunction(0x134510,&cursor510); rt.replaceFunction(0x134524,&cursor524);
}






