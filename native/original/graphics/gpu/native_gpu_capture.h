#pragma once
#include "runtime/gs/gs_backend.h"
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <array>
#include <stdexcept>

// Versioned same-build diagnostic format. No pointers or executable data.
namespace DisneyCapture {
enum Op:uint32_t {Init=1,Reset,Submit,Clut,Transfer,Upload,Flush,TexFlush,Sync,Present,Clear,Write};
struct Header {char magic[8]={'D','I','S','G','S','0','0','1'};uint32_t batch=sizeof(GSPrimitiveBatch),request=sizeof(GSPresentationRequest),context=sizeof(GSContext),transfer=sizeof(GSTransferCommand);};
struct ClutData {GSTex0Reg tex;GSTexClutReg clut;};
struct ClearData {GSContext context;uint32_t rgba;};
struct WriteData {uint32_t psm,base,bw,x,y,value;};
class Writer {
 std::ofstream f;size_t total=0;bool stopped=false;
 public:
 Writer(){if(const char*p=std::getenv("DISNEY_GPU_CAPTURE")){f.open(std::filesystem::u8path(p),std::ios::binary);if(!f)throw std::runtime_error("Cannot create requested GS capture");Header h;f.write(reinterpret_cast<const char*>(&h),sizeof(h));}}
 void record(Op op,const void* data=nullptr,uint32_t size=0){
  if(!f.is_open()||stopped)return;
  if(total+size>2ull*1024ull*1024ull*1024ull){f.flush();stopped=true;return;}
  const uint32_t head[2]={op,size};f.write(reinterpret_cast<const char*>(head),sizeof(head));if(size)f.write(reinterpret_cast<const char*>(data),size);total+=size+8;
  if(op==Present)f.flush();
 }
 template<class T> void record(Op op,const T& t){static_assert(std::is_trivially_copyable_v<T>);record(op,&t,sizeof(t));}
};
}
