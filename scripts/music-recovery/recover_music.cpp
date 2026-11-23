// Offline PCM conversion of the user's own SLUS_206.07 MUSIC.WAD.
// Does not execute PS2 code or modify the source archive.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdlib>
namespace fs=std::filesystem;
using Bytes=std::vector<uint8_t>;
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
Bytes read(const fs::path& p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Cannot open input");auto n=f.tellg();Bytes b(static_cast<size_t>(n));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),b.size()))throw std::runtime_error("Short input");return b;}
struct Channel{int prev=0,prev2=0;uint64_t clipping=0;};
void block(const uint8_t* b,Channel& c,int16_t* out){
 const int shift=b[0]&15,filter=b[0]>>4;constexpr int coeff[5][2]={{0,0},{60,0},{115,-52},{98,-55},{122,-60}};
 if(shift>12 || filter>4)throw std::runtime_error("Invalid PSX ADPCM frame");
 for(unsigned i=0;i<28;++i){int n=(b[2+i/2]>>(i%2*4))&15;if(n>=8)n-=16;int value=((n*4096)>>shift)+((c.prev*coeff[filter][0]+c.prev2*coeff[filter][1]+32)>>6);c.clipping+=value>32767||value< -32768;value=std::clamp(value,-32768,32767);c.prev2=c.prev;c.prev=value;out[i]=static_cast<int16_t>(value);}
}
void w16(std::ostream& f,uint16_t v){f.put(char(v));f.put(char(v>>8));}
void w32(std::ostream& f,uint32_t v){w16(f,uint16_t(v));w16(f,uint16_t(v>>16));}
int wmain(int argc,wchar_t** argv){try{
 if(argc<3)throw std::runtime_error("Usage: recover_music project output [track]");
 fs::path p=argv[1],out=argv[2];fs::create_directories(out);const std::wstring wanted=argc>3?argv[3]:L"";
 const wchar_t* gameDirectory=_wgetenv(L"DISNEY_GAME_DIR");
 const fs::path game=gameDirectory&&*gameDirectory?fs::path(gameDirectory):p/L"original/extracted";
 auto hed=read(game/L"MUSIC/MUSIC.HED");std::ifstream wad(game/L"MUSIC/MUSIC.WAD",std::ios::binary);
 if(!wad)throw std::runtime_error("Missing MUSIC.WAD");
 for(size_t at=0;at+8<=hed.size();){uint32_t offset=u32(&hed[at]),size=u32(&hed[at+4]);if(offset==0xffffffffu)break;
  size_t end=at+8;while(end<hed.size() && hed[end])++end;if(end==hed.size())throw std::runtime_error("Invalid HED name");
  std::string name(reinterpret_cast<char*>(&hed[at+8]),end-at-8);at=(end+4)&~size_t(3);name=name.substr(name.find_last_of("/\\")+1);
  std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(c>='A'&&c<='Z'?c+32:c);});
  if(!wanted.empty() && std::wstring(name.begin(),name.end())!=wanted)continue;
  // HED stores logical compressed length. Physical channel blocks are padded
  // to 0x30000 pairs; truncating the input at HED length loses the right tail.
  if(size%32)throw std::runtime_error("Odd stereo stream size");const size_t interleave=0x18000;
  const size_t physical=(static_cast<size_t>(size)+interleave*2-1)/(interleave*2)*(interleave*2);
  Bytes raw(physical);wad.seekg(offset);if(!wad.read(reinterpret_cast<char*>(raw.data()),raw.size()))throw std::runtime_error("WAD range outside archive");
  std::vector<int16_t> pcm;pcm.reserve(physical/16*28);Channel channels[2];
  for(size_t pos=0;pos<raw.size();){const size_t span=interleave;
   for(size_t i=0;i<span;i+=16){int16_t a[28],b[28];block(&raw[pos+i],channels[0],a);block(&raw[pos+span+i],channels[1],b);for(unsigned j=0;j<28;++j){pcm.push_back(a[j]);pcm.push_back(b[j]);}}
   pos+=span*2;
  }
  pcm.resize(static_cast<size_t>(size)/16*28);
  const auto path=out/(name+".wav");std::ofstream f(path,std::ios::binary);uint32_t dataBytes=static_cast<uint32_t>(pcm.size()*2);
  f.write("RIFF",4);w32(f,36+dataBytes);f.write("WAVEfmt ",8);w32(f,16);w16(f,1);w16(f,2);w32(f,48000);w32(f,192000);w16(f,4);w16(f,16);f.write("data",4);w32(f,dataBytes);f.write(reinterpret_cast<const char*>(pcm.data()),dataBytes);if(!f)throw std::runtime_error("Cannot write PCM");
  std::cout<<name<<"\t"<<offset<<"\t"<<size<<"\t"<<pcm.size()/2<<"\t48000\t"<<channels[0].clipping+channels[1].clipping<<"\n";
 }
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
