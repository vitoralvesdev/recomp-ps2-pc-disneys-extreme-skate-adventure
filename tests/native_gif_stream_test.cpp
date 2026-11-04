#include "ps2_runtime.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
const uint32_t g_ps2RecompiledFunctionTableBase=0,g_ps2RecompiledFunctionTableEnd=0,g_ps2RecompiledFunctionTableSlotCount=0;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[1]={};
std::vector<std::vector<uint8_t>> load(const std::filesystem::path&p) {
    std::ifstream f(p,std::ios::binary);std::vector<std::vector<uint8_t>> packets;
    uint32_t size;
    while(f.read(reinterpret_cast<char*>(&size),4)) {
        if(size>16u*1024u*1024u)throw std::runtime_error("Invalid trace record");
        auto &bytes=packets.emplace_back(size);
        if(!f.read(reinterpret_cast<char*>(bytes.data()),size))throw std::runtime_error("Incomplete trace record");
    }
    return packets;
}
int wmain(int argc,wchar_t**argv) {
 try {
    if(argc!=2)throw std::runtime_error("Usage: DisneyNativeGifStreamTest <retained native GIF trace>");
    const auto captured=load(argv[1]);
    if(captured.size()<28)throw std::runtime_error("Missing actual loading transfers");
    auto memory=std::make_unique<PS2Memory>();if(!memory->initialize())return 1;
    auto actual=std::make_unique<GS>();actual->init(memory->getGSVRAM(),4u*1024*1024,&memory->gs());
    auto reference=std::make_unique<GS>();std::vector<uint8_t> referenceVram(4u*1024*1024);GSRegisters referenceRegs{};
    reference->init(referenceVram.data(),static_cast<uint32_t>(referenceVram.size()),&referenceRegs);
    GifArbiter arbiter([&](const uint8_t*p,uint32_t n){actual->processGIFPacket(p,n);});
    memory->setGifArbiter(&arbiter);
    const uint64_t signal[]={0x1000000000008001ull,0xeull,0x100000001ull,0x60ull};
    bool interleaved=false;
    for(unsigned i=20;i<28;i+=2) {
        const auto &header=captured[i],&pixels=captured[i+1];
        if(header.size()!=96 || pixels.size()!=286720)throw std::runtime_error("Trace does not contain the proven split loading image");
        std::vector<uint8_t> merged=header;merged.insert(merged.end(),pixels.begin(),pixels.end());
        reference->processGIFPacket(merged.data(),static_cast<uint32_t>(merged.size()));
        memory->submitGifPacket(GifPathId::Path3,header.data(),static_cast<uint32_t>(header.size()));
        if(!interleaved) {
            memory->submitGifPacket(GifPathId::Path1,reinterpret_cast<const uint8_t*>(signal),sizeof(signal));
            if(!(memory->gs().csr.load()&1) || (memory->gs().siglblid&1)!=1)
                throw std::runtime_error("Independent PATH1 SIGNAL was blocked by pending PATH3 IMAGE");
            interleaved=true;
        }
        memory->submitGifPacket(GifPathId::Path3,pixels.data(),static_cast<uint32_t>(pixels.size()));
        if(std::memcmp(memory->getGSVRAM(),referenceVram.data(),referenceVram.size()))
            throw std::runtime_error("Fragmented original loading image differs from contiguous transfer");
    }
    const auto state=actual->getDebugSnapshot();
    if(state.ctx[0].tex0.tbp0!=0 || state.ctx[0].tex0.tbw!=0 || state.ctx[0].tex0.psm!=0)
        throw std::runtime_error("Pixel data was interpreted as TEX0 register commands");
    // The packet interface also accepts a tag split before its full 16-byte header.
    memory->gs().csr.fetch_and(~1ull);
    const auto *bytes=reinterpret_cast<const uint8_t*>(signal);
    memory->submitGifPacket(GifPathId::Path2,bytes,8);
    if(memory->gs().csr.load()&1)throw std::runtime_error("Incomplete header executed early");
    memory->submitGifPacket(GifPathId::Path2,bytes+8,sizeof(signal)-8);
    if(!(memory->gs().csr.load()&1))throw std::runtime_error("Split header SIGNAL was lost");
    // A finite END chain with QWC=0 still completes DMA and raises its channel IRQ.
    // No GIF/VIF payload is required for the transfer itself to finish.
    for (const uint32_t channel : {0x10008000u,0x10009000u,0x1000a000u}) {
        auto empty=std::make_unique<PS2Memory>();if(!empty->initialize())return 1;
        GifArbiter sink([](const uint8_t*,uint32_t){throw std::runtime_error("Empty DMA emitted a GIF packet");});
        empty->setGifArbiter(&sink);
        empty->write32(0x2000u,0x70000000u); // END tag, zero QWC.
        empty->write32(0x1000e000u,1u);
        empty->write32(channel+0x30u,0x2000u);
        empty->write32(channel,0x105u);
        const auto causes=empty->consumeCompletedDmacCauses();
        const auto expected=(channel-0x10008000u)/0x1000u;
        if(causes.size()!=1 || causes.front()!=expected || (empty->read32(channel)&0x100u) || !(empty->read32(0x1000e010u)&(1u<<expected)))
            throw std::runtime_error("Empty END DMA chain lost completion on channel "+std::to_string(expected));
    }
    std::cout<<"PASS: four actual split loading uploads match all 4194304 VRAM bytes; pixels do not alter TEX0; independent PATH1 SIGNAL and split headers work; zero-payload END chains complete on VIF0/VIF1/GIF\n";
    return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
