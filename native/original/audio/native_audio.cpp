// Named PCM mixer. Original event/terrain/script selection stays in original EE AOT.
// Signatures are evidenced by analysis/audio-platform-functions.txt and audio-stream-functions.txt.
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include "native_audio.h"
#include <raylib.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <set>

namespace {
constexpr uint32_t rate=48000, sfxVoices=44, voiceBase=44, musicVoice=46;
struct Sample { std::vector<int16_t> pcm; std::string name; uint32_t sampleRate=0,channels=0; size_t loopStart=0,loopEnd=0; size_t frames()const{return pcm.size()/channels;} };
struct Asset { std::string file; size_t loopStart=0,loopEnd=0; std::shared_ptr<Sample> data; uint32_t id=0; std::string name; };
struct Voice { std::shared_ptr<Sample> sample; double cursor=0,step=1; float left=1,right=1; bool loop=false,paused=false; };
std::mutex mutex;
std::unordered_map<std::string,Asset> assets;
std::unordered_map<uint32_t,std::shared_ptr<Sample>> samples;
std::unordered_map<std::string,std::string> aliases;
std::unordered_map<uint32_t,std::string> streamHashes;
std::array<Voice,47> voices{};
std::set<std::string> missing;
bool catalogLoaded=false,deviceReady=false;
float sfxMaster=1,streamMaster=1,musicMaster=1;
uint32_t nextId=1;
uint64_t nonzeroFrames=0,starts=0;
AudioStream device{};
std::string normalize(std::string s) {
 for(char&c:s){if(c=='\\')c='/';else if(c>='A'&&c<='Z')c+=32;}
 while(!s.empty()&&s.front()=='/')s.erase(s.begin());
 for(const char* ext:{".wav",".vag"})if(s.ends_with(ext))s.resize(s.size()-4);
 return s;
}
uint32_t originalHash(const std::string& name) {
 uint32_t crc=0xffffffff;
 for(unsigned char c:name){if(c>='A'&&c<='Z')c+=32;if(c=='/')c='\\';crc^=c;for(int i=0;i<8;++i)crc=(crc>>1)^((crc&1)?0xedb88320u:0u);}
 return crc;
}
std::string guestString(uint8_t* ram,uint32_t address) {
 address&=0x1ffffffu;if(address<0x1000||address>=0x2000000-512)return {};
 for(size_t i=0;i<512;++i){const auto c=ram[address+i];if(c==0)return std::string(reinterpret_cast<char*>(ram+address),i);if(c<32||c>126)return {};}
 return {};
}
void catalog() {
 if(catalogLoaded)return;
 std::ifstream f("assets/native/audio/catalog.tsv");
 if(!f)return;
 std::string line;
 std::unordered_map<std::string,int> counts;
 while(std::getline(f,line)) {
  std::istringstream in(line);std::string key,file,start,end;
  if(!std::getline(in,key,'\t')||!std::getline(in,file,'\t')||!std::getline(in,start,'\t')||!std::getline(in,end))continue;
  if(file.find("..")!=std::string::npos||file.rfind("assets/native/audio/",0)!=0)continue;
  key=normalize(key);assets[key]={file,std::stoull(start),std::stoull(end),{},0,key};
  const std::string space=key.substr(0,key.find('/')+1),base=key.substr(key.find_last_of('/')+1);
  const auto alias=space+base;++counts[alias];aliases[alias]=key;
 }
 for(const auto&[key,count]:counts)if(count>1)aliases.erase(key);
 std::set<uint32_t> ambiguous;
 for(const auto&[key,asset]:assets)if(key.rfind("stream/",0)==0) {
  const uint32_t hash=originalHash(key.substr(key.find_last_of('/')+1));
  if(streamHashes.contains(hash))ambiguous.insert(hash);else streamHashes[hash]=key;
 }
 for(uint32_t hash:ambiguous)streamHashes.erase(hash);
 catalogLoaded=true;
 std::fprintf(stderr,"[native-audio] %zu named PCM resources; exact paths, English USA voices\n",assets.size());
}
Asset* find(std::string space,std::string name,bool report=true) {
 catalog();name=normalize(name);
 for(const auto& prefix:{std::string("sounds/vag/"),std::string("streams/"),std::string("music/")})if(name.rfind(prefix,0)==0)name.erase(0,prefix.size());
 const auto key=space+name;auto it=assets.find(key);
 // The game hashes voice/music basenames. Alias only if the catalogue proves uniqueness.
 if(it==assets.end()&&space!="sfx/") {
  const auto base=space+name.substr(name.find_last_of('/')+1);auto a=aliases.find(base);
  if(a!=aliases.end())it=assets.find(a->second);
 }
 if(it==assets.end()){if(report&&missing.insert(key).second)std::fprintf(stderr,"[native-audio] Resource not mapped: %s\n",key.c_str());return nullptr;}
 return &it->second;
}
uint16_t u16(const uint8_t*p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
uint32_t u32(const uint8_t*p){return uint32_t(u16(p))|uint32_t(u16(p+2))<<16;}
std::shared_ptr<Sample> load(Asset* asset) {
 if(!asset)return {};
 if(asset->data)return asset->data;
 std::ifstream f(std::filesystem::u8path(asset->file),std::ios::binary|std::ios::ate);
 if(!f)return {};
 auto length=f.tellg();if(length<44||length>256*1024*1024)return {};
 std::vector<uint8_t>b(static_cast<size_t>(length));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),b.size()))return {};
 if(std::memcmp(b.data(),"RIFF",4)||std::memcmp(b.data()+8,"WAVE",4))return {};
 auto s=std::make_shared<Sample>();s->name=asset->name;size_t dataAt=0,dataSize=0;uint16_t bits=0,format=0;
 for(size_t at=12;at+8<=b.size();) {
  const uint32_t size=u32(b.data()+at+4);if(size>b.size()-at-8)return {};
  if(!std::memcmp(b.data()+at,"fmt ",4)&&size>=16){format=u16(b.data()+at+8);s->channels=u16(b.data()+at+10);s->sampleRate=u32(b.data()+at+12);bits=u16(b.data()+at+22);}
  if(!std::memcmp(b.data()+at,"data",4)){dataAt=at+8;dataSize=size;}
  at+=8ull+size+(size&1);
 }
 if(format!=1||bits!=16||s->channels<1||s->channels>2||s->sampleRate<8000||s->sampleRate>192000||!dataSize||dataSize%(s->channels*2))return {};
 s->pcm.resize(dataSize/2);std::memcpy(s->pcm.data(),b.data()+dataAt,dataSize);
 s->loopStart=std::min(asset->loopStart,s->frames());s->loopEnd=std::min(asset->loopEnd,s->frames());
 if(s->loopEnd<=s->loopStart)s->loopStart=s->loopEnd=0;
 asset->data=s;asset->id=nextId++;samples[asset->id]=s;
 return s;
}
float clean(float f,float fallback=0){return std::isfinite(f)?f:fallback;}
void volume(Voice&v,float left,float right,float pitch=0) {
 v.left=std::clamp(clean(left)*.01f,-1.f,1.f);v.right=std::clamp(clean(right)*.01f,-1.f,1.f);
 if(pitch!=0&&v.sample)v.step=std::clamp(double(clean(pitch,100))*.01,.01,4.)*v.sample->sampleRate/rate;
}
bool play(size_t voice,std::shared_ptr<Sample>s,float left,float right,float pitch,bool loop=false) {
 if(voice>=voices.size()||!s)return false;
 Voice v;v.sample=std::move(s);v.step=double(v.sample->sampleRate)/rate;v.loop=loop||v.sample->loopEnd>v.sample->loopStart;volume(v,left,right,pitch);voices[voice]=std::move(v);
 if(starts++<32)std::fprintf(stderr,"[native-audio] Start PCM %s channel=%zu frames=%zu rate=%u loop=%d\n",voices[voice].sample->name.c_str(),voice,voices[voice].sample->frames(),voices[voice].sample->sampleRate,voices[voice].loop);
 return true;
}
void finish(R5900Context*c,int32_t result=0){SET_GPR_S32(c,2,result);c->pc=GPR_U32(c,31);}
void bridge(uint8_t* ram,R5900Context*c,PS2Runtime*){disneyNativeAudioInvoke(c->pc,ram,c);}
void callback(void*buffer,unsigned int frames){disneyAudioRender(static_cast<float*>(buffer),frames);}
}

void disneyAudioRender(float*out,size_t frames) {
 std::fill(out,out+frames*2,0.f);std::lock_guard lock(mutex);
 for(size_t channel=0;channel<voices.size();++channel) {
  auto&v=voices[channel];if(!v.sample||v.paused)continue;const auto&s=*v.sample;
  const size_t end=(v.loop&&s.loopEnd)?s.loopEnd:s.frames(),begin=(v.loop&&s.loopEnd)?s.loopStart:0;
  const float master=channel<sfxVoices?sfxMaster:(channel==musicVoice?musicMaster:streamMaster);
  for(size_t frame=0;frame<frames;++frame) {
   if(v.cursor>=end) {if(v.loop&&end>begin)v.cursor=begin+std::fmod(v.cursor-begin,double(end-begin));else {v.sample.reset();break;}}
   const size_t a=static_cast<size_t>(v.cursor),b=a+1<end?a+1:(v.loop?begin:a);const float t=float(v.cursor-a);
   auto at=[&](size_t p,size_t c){return s.pcm[p*s.channels+std::min(c,size_t(s.channels-1))]/32768.f;};
   out[frame*2]+=(at(a,0)+(at(b,0)-at(a,0))*t)*v.left*master;
   out[frame*2+1]+=(at(a,1)+(at(b,1)-at(a,1))*t)*v.right*master;
   v.cursor+=v.step;
  }
 }
 for(size_t i=0;i<frames;++i){if(out[i*2]!=0||out[i*2+1]!=0)++nonzeroFrames;out[i*2]=std::clamp(out[i*2],-1.f,1.f);out[i*2+1]=std::clamp(out[i*2+1],-1.f,1.f);}
}

bool disneyNativeAudioInvoke(uint32_t at,uint8_t*ram,R5900Context*c) {
 std::lock_guard lock(mutex);
 const uint32_t a=GPR_U32(c,4),b=GPR_U32(c,5);
 switch(at) {
 case 0x13e880: { // Load platform wave: name, checksum, descriptor, permanent.
  uint32_t ptr=GPR_U32(c,6)&0x1ffffff;const std::string name=guestString(ram,a);auto*asset=find("sfx/",name);auto s=load(asset);
  if(std::getenv("DISNEY_AUDIO_TRACE")){uint32_t state=0,perm=0,temp=0;std::memcpy(&state,ram+0x300BE8,4);std::memcpy(&perm,ram+0x2FD334,4);std::memcpy(&temp,ram+0x2FD338,4);std::fprintf(stderr,"[audio-load] '%s' asset=%s state=%u perm=%u temp=%u\n",name.c_str(),asset?"found":"missing",state,perm,temp);}
  if(!s||ptr<0x1000||ptr>0x1ffffff-12){finish(c,0);break;}
  const uint32_t desc[3]={asset->id,uint32_t(s->loopEnd>s->loopStart),0x42c80000};std::memcpy(ram+ptr,desc,12);finish(c,1);break;
 }
 case 0x13ecd8: { // Left/right/pitch in f12..14; descriptor in a0, voice in a1.
  const uint32_t ptr=a&0x1ffffff;uint32_t id=0;if(ptr>=0x1000&&ptr<=0x1ffffff-12)std::memcpy(&id,ram+ptr,4);
  auto it=samples.find(id);const bool ok=b<sfxVoices&&it!=samples.end()&&play(b,it->second,c->f[12],c->f[13],c->f[14]);finish(c,ok?int32_t(b):-1);break;
 }
 case 0x13e8c8: if(a<sfxVoices)volume(voices[a],c->f[12],c->f[13],c->f[14]);finish(c);break;
 case 0x13e7c8: if(a<sfxVoices)voices[a]={};finish(c);break;
 case 0x13ec18: { // Original reset stops SFX, music and voice streams.
  voices={};const uint32_t zero[4]={};std::memcpy(ram+0x358ff8,zero,16);finish(c);break;
 }
 case 0x13f000: {uint32_t free=0;const uint32_t base=a==1?22:0;for(unsigned i=0;i<22;++i)if(!voices[base+i].sample)free|=1u<<i;finish(c,free);break;}
 case 0x13f0d8: finish(c,a<sfxVoices&&bool(voices[a].sample));break;
 case 0x13e500: sfxMaster=std::clamp(clean(c->f[12])*.01f,0.f,1.f);std::memcpy(ram+0x2fd318,&c->f[12],4);streamMaster=sfxMaster;finish(c);break;
 case 0x11a088: streamMaster=std::clamp(float(int32_t(a))*.01f,0.f,1.f);finish(c);break;
 case 0x11a220: {auto s=load(find("music/",guestString(ram,a)));finish(c,play(musicVoice,s,100,100,100));break;}
 case 0x11a398: { // a0 is the original basename CRC, not a string pointer (26db48).
  catalog();auto entry=streamHashes.find(a);Asset*asset=entry==streamHashes.end()?nullptr:&assets.at(entry->second);
  if(asset&&starts<32)std::fprintf(stderr,"[native-audio] Voice event %08x -> %s\n",a,entry->second.c_str());
  auto s=load(asset);finish(c,b<2&&play(voiceBase+b,s,c->f[12],c->f[13],c->f[14]));break;
 }
 case 0x11a628: voices[musicVoice]={};finish(c);break;
 case 0x11a728: if(a<2)voices[voiceBase+a]={};finish(c);break;
 case 0x11a898: finish(c,voices[musicVoice].sample?2:1);break;
 case 0x11a8c8: finish(c,a<2?(voices[voiceBase+a].sample?2:1):0);break;
 case 0x11aab0: finish(c,find(b==1?"music/":"stream/",guestString(ram,a),false)!=nullptr);break;
 case 0x11a168: if(a<2)volume(voices[voiceBase+a],c->f[12],c->f[13]);finish(c,1);break;
 case 0x11a0e8: musicMaster=std::clamp(clean(c->f[12])*.01f,0.f,1.f);finish(c,1);break;
 case 0x11a918: if(b==1)voices[musicVoice].paused=a!=0;else for(unsigned i=voiceBase;i<musicVoice;++i)voices[i].paused=a!=0;finish(c);break;
 case 0x11a998: if(b<2)voices[voiceBase+b].paused=a!=0;finish(c);break;
 default:return false;
 }
 return true;
}
void registerDisneyNativeAudio(PS2Runtime&r) {
 for(uint32_t pc:{0x13e880u,0x13ecd8u,0x13e8c8u,0x13e7c8u,0x13ec18u,0x13f000u,0x13f0d8u,0x13e500u,0x11a088u,0x11a220u,0x11a398u,0x11a628u,0x11a728u,0x11a898u,0x11a8c8u,0x11aab0u,0x11a168u,0x11a0e8u,0x11a918u,0x11a998u})r.registerFunction(pc,&bridge);
 std::fprintf(stderr,"[native-audio] Original sound events connected to Windows PCM mixer\n");
}
void disneyAudioInitialize() {
 if(deviceReady||!IsAudioDeviceReady())return;
 SetAudioStreamBufferSizeDefault(1024);device=LoadAudioStream(rate,32,2);
 if(!IsAudioStreamValid(device)){std::fprintf(stderr,"[native-audio] Windows output stream unavailable\n");return;}
 SetAudioStreamCallback(device,&callback);deviceReady=true;PlayAudioStream(device);
 std::fprintf(stderr,"[native-audio] Windows output 48000 Hz stereo, 44 SFX + 2 voices + music\n");
}
void disneyAudioShutdown() {
 if(deviceReady){StopAudioStream(device);UnloadAudioStream(device);deviceReady=false;}
 std::lock_guard lock(mutex);voices={};samples.clear();assets.clear();aliases.clear();streamHashes.clear();catalogLoaded=false;
 std::fprintf(stderr,"[native-audio] %llu starts, %llu nonzero mixed frames\n",(unsigned long long)starts,(unsigned long long)nonzeroFrames);
}
void disneyAudioResetForTest(){std::lock_guard lock(mutex);voices={};sfxMaster=streamMaster=musicMaster=1;nonzeroFrames=0;}
uint64_t disneyAudioRenderedNonzeroFrames(){std::lock_guard lock(mutex);return nonzeroFrames;}
