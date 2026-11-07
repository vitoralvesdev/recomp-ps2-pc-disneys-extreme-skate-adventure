#pragma once
// SLUS_206.07 bounded progress probes. Off unless DISNEY_HANG_DIAGNOSTICS is set: the per-job VU1 capture they switch on
// costs frame time, so a normal play session runs without them and a "diagnostico" launch turns them on.
#include <atomic>
#include <array>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <direct.h>
#include "runtime/ee_scheduler.h"
extern "C" __declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void *, char *, unsigned long);
extern "C" __declspec(dllimport) void *__stdcall GetCurrentThread(void);
struct DiagnosticFileTime {unsigned long low,high;};
extern "C" __declspec(dllimport) int __stdcall GetThreadTimes(void *, DiagnosticFileTime *, DiagnosticFileTime *, DiagnosticFileTime *, DiagnosticFileTime *);
namespace DisneyHang {
inline bool enabled(){static const bool value=std::getenv("DISNEY_HANG_DIAGNOSTICS")!=nullptr;return value;}
// Copying 32 KB of VU1 code/data per MSCAL job distorts any timing profile,
// so the last-job capture is a separate opt-in (DISNEY_VU_CAPTURE).
inline bool vuCapture(){static const bool value=std::getenv("DISNEY_VU_CAPTURE")!=nullptr;return value;}
// where a stall dump is written: DISNEY_HANG_OUTPUT, else hang-reports next to the executable
inline const char*outputDir(){
 static std::string dir;
 if(dir.empty()){
  if(const char*env=std::getenv("DISNEY_HANG_OUTPUT"))dir=env;
  else{char path[32768];const auto n=GetModuleFileNameA(nullptr,path,sizeof(path));if(n&&n<sizeof(path)){char*slash=std::strrchr(path,'\\');if(slash)*slash=0;dir=std::string(path)+"\\hang-reports";}}
  if(!dir.empty()&&_mkdir(dir.c_str())!=0&&errno!=EEXIST)dir.clear();
 }
 return dir.empty()?nullptr:dir.c_str();
}
inline std::array<std::atomic<unsigned long long>,0x80000> frequency{};
// Wall time charged to the guest function that was executing when the clock
// was read. Two clock reads per dispatch; only enabled with diagnostics.
inline std::array<std::atomic<unsigned long long>,0x80000> nsByPc{};
// Time the scheduler spends between a guest function returning and the next
// dispatch (event polling, debug publish, checkpoint, table lookup).
inline std::atomic<unsigned long long> schedulerNs{0};
inline thread_local std::chrono::steady_clock::time_point bodyEnd{};
inline std::array<std::atomic<uint32_t>,128> ring{};
inline std::atomic<unsigned long long> dispatches{0}, logic{0}, draw{0}, inputLogic{0}, flip{0}, padReads{0}, padDone{0}, rpc{0}, rpcDone{0};
// Wall-clock attribution: guest-thread CPU time separates "slow emulation"
// from "waiting on the host" without sampling profilers.
inline std::atomic<unsigned long long> gameCpu100ns{0}, vu0Calls{0}, vu0Ns{0}, stubs100ns{0};
inline std::atomic<uint32_t> lastPc{0},lastRa{0},lastSp{0},gameTid{0},lastPad{0xffff},deviceAddr{0}, physical{0},physicalEdges{0},keyboardEdges{0},renderPhase{0};
inline std::atomic<unsigned long long> hostFrames{0}, uploadUs{0}, lastUploadUs{0};
inline std::array<std::atomic<unsigned long long>,64> irqRequested{},irqQueued{};
inline std::atomic<unsigned long long> queueHandler{0};
inline std::atomic<uint8_t*> ramPointer{nullptr};
inline thread_local uint32_t activePc=0;
struct Event {std::atomic<unsigned long long> seq{0},oldCsr{0},value{0};std::atomic<uint32_t> kind{0},pc{0},head{0},busy{0};};
inline std::array<Event,1024> events{};
inline std::atomic<unsigned long long> eventSeq{0},signals{0},signalWhilePending{0};
inline void gsEvent(uint32_t kind,unsigned long long oldCsr,unsigned long long value){
 if(!enabled())return;
 const auto seq=eventSeq.fetch_add(1)+1;auto& e=events[seq&1023];e.seq=0;e.kind=kind;e.oldCsr=oldCsr;e.value=value;e.pc=activePc;
 auto* ram=ramPointer.load();uint32_t head=0,busy=0;if(ram){std::memcpy(&head,ram+0x346b30,4);std::memcpy(&busy,ram+0x346b34,4);}e.head=head;e.busy=busy;e.seq=seq;
 if(kind==1){++signals;if(oldCsr&1)++signalWhilePending;}
}
inline void dumpGsEvents(const char* suffix=""){
 const char* directory=outputDir();if(!directory)return;char path[1024];std::snprintf(path,sizeof(path),"%s/gs-events%s.txt",directory,suffix);FILE* f=std::fopen(path,"wb");if(!f)return;
 auto end=eventSeq.load();std::fprintf(f,"signals=%llu signalWhilePending=%llu events=%llu\n",signals.load(),signalWhilePending.load(),end);
 for(auto seq=end>1023?end-1023:1;seq<=end;++seq){auto& e=events[seq&1023];if(e.seq.load()==seq)std::fprintf(f,"%llu kind=%u pc=%08x oldCsr=%llx value=%llx head=%08x busy=%08x\n",seq,e.kind.load(),e.pc.load(),e.oldCsr.load(),e.value.load(),e.head.load(),e.busy.load());}
 std::fclose(f);
}
inline void dispatch(uint32_t pc) {
 // Frame flip rate is the cheapest always-on progress metric: one compare per
 // EE dispatch, and it is the only counter a clean (no diagnostics) session has.
 if(pc==0x166b28u)++flip;
 if(!enabled())return;
 {
  static thread_local uint32_t profilePc=0;
  static thread_local std::chrono::steady_clock::time_point profileStart{};
  const auto now=std::chrono::steady_clock::now();
  if(profilePc>=0x100000u&&profilePc<0x300000u)
   nsByPc[(profilePc-0x100000u)/4].fetch_add((unsigned long long)std::chrono::duration_cast<std::chrono::nanoseconds>(now-profileStart).count(),std::memory_order_relaxed);
  profilePc=pc;profileStart=now;
 }
 activePc=pc;
 thread_local unsigned long long calls=0;
 ++calls;
 if(pc==0x1f9ef8u)++logic;
 if(pc==0x1fb1a0u)++draw;
 if(pc==0x1f9b00u)++inputLogic;
 if(pc>=0x15a400u && pc<0x15a900u)++queueHandler;
 if((calls&63u)!=0)return;
 dispatches.fetch_add(64,std::memory_order_relaxed);
 lastPc.store(pc,std::memory_order_relaxed);
 auto seq=dispatches.load(std::memory_order_relaxed)/64;
 ring[seq&127].store(pc,std::memory_order_relaxed);
 if(pc>=0x100000 && pc<0x300000)frequency[(pc-0x100000)/4].fetch_add(1,std::memory_order_relaxed);
}
inline void context(const R5900Context& c){
 if(!enabled())return;
 static thread_local unsigned tick=0;
 if((tick++&1023u)==0u){
  DiagnosticFileTime created,exited,kernel,user{};
  if(GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user))
   gameCpu100ns.store((((unsigned long long)kernel.high<<32)|kernel.low)+(((unsigned long long)user.high<<32)|user.low),std::memory_order_relaxed);
 }
 gameTid.store(GetCurrentThreadId(),std::memory_order_relaxed);
 lastRa.store(getRegU32(&c,31),std::memory_order_relaxed);
 lastSp.store(getRegU32(&c,29),std::memory_order_relaxed);
}
// Called around guest-stub bridges: time the EE thread spends inside handlers
// that are host code (audio, pad, I/O), which never shows up in guest PCs.
struct StubTimer {
 std::chrono::steady_clock::time_point start;
 StubTimer():start(std::chrono::steady_clock::now()){}
 ~StubTimer(){stubs100ns.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count()/100ull,std::memory_order_relaxed);}
};
inline void vu0Timing(std::chrono::steady_clock::duration d){
 vu0Calls.fetch_add(1,std::memory_order_relaxed);
 vu0Ns.fetch_add((unsigned long long)std::chrono::duration_cast<std::chrono::nanoseconds>(d).count(),std::memory_order_relaxed);
}
inline void frame(PS2Runtime& rt){
 if(!enabled())return;
 ++hostFrames;
 static const auto start=std::chrono::steady_clock::now();
 static auto last=start;
 auto now=std::chrono::steady_clock::now();
 if(now-last<std::chrono::seconds(1))return;
 last=now;
 const auto s=rt.eeScheduler().snapshot();
 static unsigned long long prevCpu=0,prevVu0=0,prevStub=0;static auto prevSample=now;
 const double sampleSeconds=std::max(1e-3,std::chrono::duration<double>(now-prevSample).count());prevSample=now;
 const auto cpu=gameCpu100ns.load(),vu0=vu0Ns.load(),stub=stubs100ns.load();
 const double cpuPercent=(cpu-prevCpu)*1e-7/sampleSeconds*100.0;
 const double vu0Percent=(vu0-prevVu0)*1e-9/sampleSeconds*100.0;
 const double stubPercent=(stub-prevStub)*1e-7/sampleSeconds*100.0;
 prevCpu=cpu;prevVu0=vu0;prevStub=stub;
 std::fprintf(stderr,"[hang-progress] t=%.3f hostFrames=%llu dispatchApprox=%llu logicEntry=%llu drawEntry=%llu inputLogic=%llu flipEntry=%llu padReads=%llu padDone=%llu vblank=%llu pc=%08x ra=%08x sp=%08x tid=%u busy=%08x dma=%llu gif=%llu gs=%llu vif=%llu cycle=%llu rpc=%llu/%llu input=%04x physical=%08x edges=%u/%u uploadUs=%llu cpu=%.0f%% vu0=%llu/%.0f%% stub=%.0f%%\n",
 std::chrono::duration<double>(now-start).count(),hostFrames.load(),dispatches.load(),logic.load(),draw.load(),inputLogic.load(),flip.load(),padReads.load(),padDone.load(),
 rt.memory().gs().vsyncTick.load(),lastPc.load(),lastRa.load(),lastSp.load(),gameTid.load(),rt.memory().read32(0x346b34),
 (unsigned long long)rt.memory().dmaStartCount(),(unsigned long long)rt.memory().gifCopyCount(),(unsigned long long)rt.memory().gsWriteCount(),(unsigned long long)rt.memory().vifWriteCount(),(unsigned long long)s.eeCycle,rpc.load(),rpcDone.load(),lastPad.load(),physical.load(),physicalEdges.load(),keyboardEdges.load(),lastUploadUs.load(),
 cpuPercent,vu0Calls.load(),vu0Percent,stubPercent);
 for(const auto& t:s.threads)std::fprintf(stderr,"[hang-thread] id=%d status=%d wait=%d waitId=%d pc=%08x ra=%08x\n",t.id,(int)t.status,(int)t.waitReason,t.waitId,t.pc,t.ra);
 std::array<std::pair<unsigned long long,uint32_t>,12> top{};
 static std::array<unsigned long long,0x80000> previous{};
 for(uint32_t i=0;i<frequency.size();++i){auto n=frequency[i].load(std::memory_order_relaxed);auto d=n-previous[i];previous[i]=n;if(d>top.back().first){top.back()={d,0x100000+i*4};std::sort(top.begin(),top.end(),[](auto a,auto b){return a.first>b.first;});}}
 std::fprintf(stderr,"[hang-hot-sampled]");for(auto [n,pc]:top)if(n)std::fprintf(stderr," %08x:%llu",pc,n);std::fprintf(stderr,"\n");
 std::array<std::pair<unsigned long long,uint32_t>,12> topTime{};
 static std::array<unsigned long long,0x80000> previousNs{};
 unsigned long long totalNs=0;
 for(uint32_t i=0;i<nsByPc.size();++i){
  auto n=nsByPc[i].load(std::memory_order_relaxed);auto d=n-previousNs[i];previousNs[i]=n;totalNs+=d;
  if(d>topTime.back().first){topTime.back()={d,0x100000+i*4};std::sort(topTime.begin(),topTime.end(),[](auto a,auto b){return a.first>b.first;});}
 }
 static unsigned long long previousSched=0;
 const unsigned long long schedNow=schedulerNs.load();const unsigned long long schedDelta=schedNow-previousSched;previousSched=schedNow;
 std::fprintf(stderr,"[hang-hot-time] totalMs=%llu schedMs=%llu",totalNs/1000000ull,schedDelta/1000000ull);
 for(auto [n,pc]:topTime)if(n)std::fprintf(stderr," %08x:%.1fms",pc,n/1000000.0);
 std::fprintf(stderr,"\n");
 std::fprintf(stderr,"[hang-dma]");for(uint32_t a:{0x10008000u,0x10009000u,0x1000a000u,0x1000b000u,0x1000b400u,0x1000c000u,0x1000c400u})std::fprintf(stderr," %08x=%08x",a,rt.memory().read32(a));std::fprintf(stderr,"\n");
 std::fprintf(stderr,"[hang-queue] csr=%llx imr=%llx sig=%llx head=%08x busy=%08x flags=%u/%u dstat=%08x vifstat=%08x vuPc=%x vuCycles=%llu vi2=%04x stop=%d/%d handlerDispatch=%llu irqGS=%llu/%llu irqGIF=%llu/%llu irqVIF=%llu/%llu\n",(unsigned long long)rt.memory().gs().csr.load(),(unsigned long long)rt.memory().gs().imr,(unsigned long long)rt.memory().gs().siglblid,rt.memory().read32(0x346b30),rt.memory().read32(0x346b34),rt.memory().read32(0x362610),rt.memory().read32(0x362614),rt.memory().read32(0x1000e010),rt.memory().read32(0x10003c00),rt.vu1().state().pc,(unsigned long long)rt.vu1().state().cycles,rt.vu1().state().vi[2]&65535,rt.vu1().state().stoppedByD,rt.vu1().state().stoppedByT,queueHandler.load(),irqRequested[0].load(),irqQueued[0].load(),irqRequested[34].load(),irqQueued[34].load(),irqRequested[33].load(),irqQueued[33].load());
 static uint32_t previousBusy=0;static unsigned stable=0;static unsigned dumps=0;
 const uint32_t busy=rt.memory().read32(0x346b34);stable=(busy && busy==previousBusy)?stable+1:0;previousBusy=busy;
 // five seconds on one outstanding render-queue node: the guest is spin-waiting for a GPU completion the host never gave it
 if(stable>=5&&dumps<3u){
  ++dumps;char suffix[16];std::snprintf(suffix,sizeof(suffix),"%u",dumps);
  dumpGsEvents(suffix);
  if(const char* directory=outputDir()){
   char path[1024];std::snprintf(path,sizeof(path),"%s/stalled%s.ram",directory,suffix);if(FILE* f=std::fopen(path,"wb")){std::fwrite(rt.memory().getRDRAM(),1,PS2_RAM_SIZE,f);std::fclose(f);}
   std::snprintf(path,sizeof(path),"%s/stalled%s-vu1-code.bin",directory,suffix);if(FILE* f=std::fopen(path,"wb")){std::fwrite(rt.memory().getVU1Code(),1,0x4000,f);std::fclose(f);}
   std::snprintf(path,sizeof(path),"%s/stalled%s-vu1-data.bin",directory,suffix);if(FILE* f=std::fopen(path,"wb")){std::fwrite(rt.memory().getVU1Data(),1,0x4000,f);std::fclose(f);}
   std::snprintf(path,sizeof(path),"%s/stalled%s-gs.txt",directory,suffix);if(FILE* f=std::fopen(path,"wb")){const auto&r=rt.memory().gs();
    std::fprintf(f,"busy=%08x head=%08x pmode=%llx smode2=%llx dispfb1=%llx display1=%llx dispfb2=%llx display2=%llx csr=%llx imr=%llx siglblid=%llx\n",
     busy,rt.memory().read32(0x346b30),(unsigned long long)r.pmode,(unsigned long long)r.smode2,(unsigned long long)r.dispfb1,(unsigned long long)r.display1,
     (unsigned long long)r.dispfb2,(unsigned long long)r.display2,(unsigned long long)r.csr.load(),(unsigned long long)r.imr,(unsigned long long)r.siglblid);
    std::fclose(f);}
   std::fprintf(stderr,"[hang-evidence] stalled render queue: saved RAM, VU1 and GS state to %s (dump %u)\n",directory,dumps);
  }
 }
}
}

