#include "native_gif_stream.h"
#include <array>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <mutex>
#include <cstdio>
#include <algorithm>
namespace disney::original {
namespace {
using Paths=std::array<std::vector<uint8_t>,4>;
std::unordered_map<const void*,Paths> streams;
// The sink runs under the lock so carried-buffer spans stay valid; recursive so a sink that feeds another GIF
// packet back through the same owner (GS-internal submission) cannot self-deadlock.
std::recursive_mutex streamMutex;

bool gifPacketSize(const uint8_t *data,size_t available,size_t &needed) {
    if(available<16)return false;
    uint64_t tag;std::memcpy(&tag,data,8);
    const size_t loops=tag&0x7fff;
    const unsigned format=(tag>>58)&3;
    const size_t registers=((tag>>60)&15)?((tag>>60)&15):16;
    size_t body=loops*16;
    if(format==0)body=loops*registers*16;
    else if(format==1)body=(loops*registers*8+15)&~size_t(15);
    needed=16+body;
    return true;
}
}
void diagnoseGifStreams(const void* owner) {
 std::lock_guard lock(streamMutex);
 auto found=streams.find(owner);
 if(found==streams.end()){std::fprintf(stderr,"[hang-gif-stream] no pending owner\n");return;}
 for(unsigned path=1;path<=3;++path){
 const auto& p=found->second[path];uint64_t tag=0;if(p.size()>=8)std::memcpy(&tag,p.data(),8);
 size_t needed=0;gifPacketSize(p.data(),p.size(),needed);
 std::fprintf(stderr,"[hang-gif-stream] path=%u pending=%zu needed=%zu tag=%016llx prefix=",path,p.size(),needed,(unsigned long long)tag);
 for(size_t i=0;i<std::min<size_t>(p.size(),48);++i)std::fprintf(stderr,"%02x",p[i]);std::fprintf(stderr,"\n");
 }
}
void resetGifStreams(const void *owner) { std::lock_guard lock(streamMutex);streams.erase(owner); }
void assembleGifPackets(const void *owner,uint8_t path,const uint8_t *bytes,uint32_t size,GifPacketSink sink,void *ctx) {
    if(path<1 || path>3)throw std::invalid_argument("Invalid native GIF path");
    if(!bytes || size==0)return;
    std::lock_guard lock(streamMutex);
    auto found=streams.find(owner);
    std::vector<uint8_t> *pending=(found!=streams.end() && !found->second[path].empty())?&found->second[path]:nullptr;
    if(!pending) {
        // Nothing carried on this path: frame the caller's buffer in place.
        size_t consumed=0;
        while(size-consumed>=16) {
            size_t needed=0;
            if(!gifPacketSize(bytes+consumed,size-consumed,needed) || size-consumed<needed)break;
            sink(ctx,bytes+consumed,static_cast<uint32_t>(needed));
            consumed+=needed;
        }
        if(consumed<size)
            streams[owner][path].assign(bytes+consumed,bytes+size);
        return;
    }
    pending->insert(pending->end(),bytes,bytes+size);
    size_t consumed=0;
    while(pending->size()-consumed>=16) {
        size_t needed=0;
        if(!gifPacketSize(pending->data()+consumed,pending->size()-consumed,needed) || pending->size()-consumed<needed)break;
        sink(ctx,pending->data()+consumed,static_cast<uint32_t>(needed));
        consumed+=needed;
    }
    if(consumed)pending->erase(pending->begin(),pending->begin()+consumed);
}
}
