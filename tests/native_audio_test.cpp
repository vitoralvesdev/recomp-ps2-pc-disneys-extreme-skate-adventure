#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include "native_audio.h"
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <thread>
#include <chrono>
#include <raylib.h>
#include <stdexcept>
const uint32_t g_ps2RecompiledFunctionTableBase=0;
const uint32_t g_ps2RecompiledFunctionTableEnd=0;
const uint32_t g_ps2RecompiledFunctionTableSlotCount=0;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[1]={};
void check(bool ok,const char*what){if(!ok)throw std::runtime_error(what);std::cout<<"PASS "<<what<<'\n';}
double energy(const std::vector<float>&p,int channel=-1){double e=0;for(size_t i=0;i<p.size();++i)if(channel<0||int(i%2)==channel)e+=double(p[i])*p[i];return e;}
int main(int argc,char**argv){try{
 std::vector<uint8_t>ram(32*1024*1024);R5900Context c{};auto*ctx=&c;
 auto invoke=[&](uint32_t pc,uint32_t a=0,uint32_t b=0,float l=100,float r=100,float pitch=100){SET_GPR_U32(ctx,4,a);SET_GPR_U32(ctx,5,b);SET_GPR_U32(ctx,31,0x123456);c.f[12]=l;c.f[13]=r;c.f[14]=pitch;check(disneyNativeAudioInvoke(pc,ram.data(),ctx),"production bridge dispatch");check(c.pc==0x123456,"original return address preserved");return int32_t(GPR_U32(ctx,2));};
 auto name=[&](const char*s){std::strcpy(reinterpret_cast<char*>(ram.data()+0x10000),s);return 0x10000u;};
 auto load=[&](const char*s,uint32_t desc){SET_GPR_U32(&c,6,desc);return invoke(0x13e880,name(s));};
 disneyAudioResetForTest();
 if(argc>1 && std::string(argv[1])=="--music-contract"){
  std::ifstream catalog("assets/native/audio/catalog.tsv");std::string line;unsigned tested=0;
  while(std::getline(catalog,line)){
   const auto tab=line.find('\t');const auto key=line.substr(0,tab);if(key.rfind("music/",0)!=0)continue;
   disneyAudioShutdown();disneyAudioResetForTest();
   check(invoke(0x11a220,name(key.c_str()))==1,"original music path loads through production bridge");
   check(invoke(0x11a898)==2,"music reports active");
   std::vector<float> pcm(48000*2);double total=0;unsigned seconds=0;
   while(seconds<20 && total<.001){disneyAudioRender(pcm.data(),48000);total+=energy(pcm);++seconds;}
   check(total>.001,"music yields nonzero stereo PCM");
   invoke(0x11a628);check(invoke(0x11a898)==1,"music stop reports ready");
   disneyAudioRender(pcm.data(),48000);check(energy(pcm)==0,"music stop leaves no stuck samples");
   std::cout<<"MUSIC_PASS "<<key<<" secondsToAudio="<<seconds<<" energy="<<total<<'\n';++tested;
  }
  check(tested==37,"all 37 music and ambience entries exercised");
  disneyAudioShutdown();disneyAudioResetForTest();
  if(argc>2 && std::string(argv[2])=="--device"){
   InitAudioDevice();check(IsAudioDeviceReady(),"Windows music endpoint initialized");disneyAudioInitialize();
   check(invoke(0x11a220,name("music/spinning_around"))==1,"recovered soundtrack starts on Windows device");
   std::this_thread::sleep_for(std::chrono::seconds(2));
   check(disneyAudioRenderedNonzeroFrames()>1000,"Windows callback consumed nonzero recovered soundtrack PCM");
   std::cout<<"MUSIC_DEVICE_NONZERO_FRAMES "<<disneyAudioRenderedNonzeroFrames()<<'\n';disneyAudioShutdown();CloseAudioDevice();
  }
  std::cout<<"NATIVE MUSIC CONTRACT PASSED tracks="<<tested<<'\n';return 0;
 }
 check(load("Shared\\Menu\\MenuSelect",0x20000)==1,"MenuSelect exact path loads");
 check(load("Shared\\Menu\\DoesNotExist",0x20100)==0,"unknown name fails without unrelated-sound fallback");
 check(invoke(0x13ecd8,0x20000,0)==0,"original menu event starts voice 0");
 std::vector<float>one(48000*2);disneyAudioRender(one.data(),48000);check(energy(one)>.001,"real menu PCM reaches native stereo output");
 check(invoke(0x13f0d8,0)==0,"one-shot voice ends according to consumed audio frames");
 invoke(0x13ecd8,0x20000,0,100,100,0);std::vector<float>base(one.size());disneyAudioRender(base.data(),48000);check(base==one,"zero/default pitch preserves original WAV sample rate");
 invoke(0x13ecd8,0x20000,0,100,100,200);std::vector<float>fast(14400*2);disneyAudioRender(fast.data(),14400);check(invoke(0x13f0d8,0)==0&&energy(fast)>.001,"200 percent pitch advances audio twice as fast");
 invoke(0x13ecd8,0x20000,0);invoke(0x13ecd8,0x20000,1);std::vector<float>two(one.size());disneyAudioRender(two.data(),48000);
 double delta=0;for(size_t i=0;i<one.size();++i)delta+=std::abs(two[i]-std::clamp(one[i]*2,-1.f,1.f));check(delta<.00001,"same sound overlaps independently on two channels");
 invoke(0x13ecd8,0x20000,0);invoke(0x13e8c8,0,0,100,0,100);std::vector<float>pan(one.size());disneyAudioRender(pan.data(),48000);check(energy(pan,0)>.001&&energy(pan,1)==0,"left/right event volumes preserved");
 invoke(0x13ecd8,0x20000,0);invoke(0x13e8c8,0,0,0,0,0);std::vector<float>silent(one.size());disneyAudioRender(silent.data(),48000);check(energy(silent)==0,"volume update mutes an existing voice");
 invoke(0x13ecd8,0x20000,0);invoke(0x13e7c8,0);check(invoke(0x13f0d8,0)==0,"stop command releases exactly the selected channel");
 check(load("Terrains\\RollWood",0x20200)==1,"PS2 terrain loop fallback loads by full path");
 invoke(0x13ecd8,0x20200,2);std::vector<float>loop(48000*8*2);disneyAudioRender(loop.data(),48000*8);check(invoke(0x13f0d8,2)==1&&energy(loop)>.001,"original terrain loop survives its first period");invoke(0x13e7c8,2);
 check(invoke(0x11a398,0x3820e30d,0)==1,"English Woody voice matched to original stream CRC 3820e30d");
 check(invoke(0x11a8c8,0)==2,"voice stream reports original active status");
 invoke(0x11a998,1,0);disneyAudioRender(silent.data(),48000);check(energy(silent)==0,"pause freezes and silences the selected stream");
 invoke(0x11a998,0,0);std::vector<float>voice(48000*8*2);disneyAudioRender(voice.data(),48000*8);check(energy(voice)>.001&&invoke(0x11a8c8,0)==1,"voice resumes, produces audio and finishes");
 check(invoke(0x11a220,name("music\\amb_hub"))==1,"HUB ambience exact name loads");
 std::vector<float>amb(48000*4*2);disneyAudioRender(amb.data(),48000*4);check(energy(amb)>.001,"HUB ambience produces stereo PCM");invoke(0x11a628);
 invoke(0x13ecd8,0x20200,2);invoke(0x11a398,0x3820e30d,0);invoke(0x13ec18);disneyAudioRender(silent.data(),48000);check(energy(silent)==0&&invoke(0x13f0d8,2)==0&&invoke(0x11a8c8,0)==1,"original stop-all releases SFX and voice streams together");
 if(argc>2&&std::string(argv[2])=="--device") {InitAudioDevice();check(IsAudioDeviceReady(),"Windows audio endpoint initialized");disneyAudioResetForTest();disneyAudioInitialize();invoke(0x11a398,0x3820e30d,0);std::this_thread::sleep_for(std::chrono::seconds(2));check(disneyAudioRenderedNonzeroFrames()>1000,"WASAPI callback consumed nonzero original voice PCM");disneyAudioShutdown();CloseAudioDevice();}
 if(argc>1){std::ofstream f(std::filesystem::u8path(argv[1]),std::ios::binary);auto w16=[&](uint16_t v){f.put(char(v));f.put(char(v>>8));};auto w32=[&](uint32_t v){w16(v);w16(v>>16);};const uint32_t bytes=uint32_t((one.size()+voice.size()+amb.size())*2);f.write("RIFF",4);w32(36+bytes);f.write("WAVEfmt ",8);w32(16);w16(1);w16(2);w32(48000);w32(192000);w16(4);w16(16);f.write("data",4);w32(bytes);for(const auto*part:{&one,&voice,&amb})for(float x:*part)w16(uint16_t(int16_t(std::clamp(x,-1.f,1.f)*32767)));}
 std::cout<<"NATIVE AUDIO CONTRACT PASSED; nonzero frames="<<disneyAudioRenderedNonzeroFrames()<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
