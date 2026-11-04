// Replays retained DMA/VIF data through native compiled VU jobs. Not a game or
// a complete GS snapshot: texture/VRAM correctness cannot be inferred here.
#include "ps2_runtime.h"
#include "../tools/PS2Recomp/ps2xRuntime/src/lib/ps2_iop_host.h"
#include "ps2x/iop/iop_subsystem.h"
#include "runtime/gs/gs_cpu_backend.h"
#include "ps2_runtime_macros.h"
#include "../tools/PS2Recomp/ps2xRuntime/src/lib/Kernel/Stubs/Pad.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>
const uint32_t g_ps2RecompiledFunctionTableBase=0;
const uint32_t g_ps2RecompiledFunctionTableEnd=0;
const uint32_t g_ps2RecompiledFunctionTableSlotCount=0;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[1]={};
namespace fs=std::filesystem;
std::vector<uint8_t> read(const fs::path &path,size_t expected) {
    std::ifstream f(path,std::ios::binary);std::vector<uint8_t> bytes(expected);
    if(!f.read(reinterpret_cast<char*>(bytes.data()),expected))throw std::runtime_error("Missing/short retained snapshot");
    return bytes;
}
uint32_t u32(const std::vector<uint8_t>&ram,uint32_t at){uint32_t v;std::memcpy(&v,ram.data()+at,4);return v;}
std::vector<uint8_t> chain(const std::vector<uint8_t>&ram,uint32_t first) {
    std::vector<uint8_t> stream;std::vector<uint32_t> stack;std::set<uint32_t> seen;
    uint32_t at=first&0x1ffffff;
    while(at+16<=ram.size() && seen.insert(at).second && seen.size()<65536) {
        uint64_t tag;std::memcpy(&tag,ram.data()+at,8);
        const uint32_t count=(tag&65535)*16,kind=(tag>>28)&7,address=(tag>>32)&0x1ffffff;
        stream.insert(stream.end(),ram.begin()+at+8,ram.begin()+at+16);
        const uint32_t data=(kind==0 || kind==3 || kind==4)?address:at+16;
        if(data>ram.size() || count>ram.size()-data)throw std::runtime_error("DMA data outside retained RAM");
        stream.insert(stream.end(),ram.begin()+data,ram.begin()+data+count);
        if(kind==0 || kind==7)return stream;
        if(kind==1)at+=16+count;
        else if(kind==2)at=address;
        else if(kind==3 || kind==4)at+=16;
        else if(kind==5){stack.push_back(at+16+count);at=address;}
        else if(kind==6){if(stack.empty())return stream;at=stack.back();stack.pop_back();}
    }
    throw std::runtime_error("Unterminated/cyclic retained DMA chain");
}
#include "native_graphics_profile.inl"
#include "native_gpu_contract.inl"
#include "native_last_vu_signal.inl"
#include "native_complete_gs_replay.inl"
#include "native_z_color_alias_test.inl"
#include "native_color_depth_clear_test.inl"
#include "native_original_pad_table_test.inl"
#include "gs_capture_inventory.inl"
#include "native_shared_depth_test.inl"
int wmain(int argc,wchar_t**argv) {
 try {
    if(argc==2 && std::wstring(argv[1])==L"--shared-depth-test")return testSharedDepthAcrossColorTargets();
    if(argc==3 && std::wstring(argv[1])==L"--gs-inventory")return inventoryGsCapture(fs::path(argv[2]));
    if(argc==3 && std::wstring(argv[1])==L"--original-pad-table-test")return testOriginalPadTable(fs::path(argv[2]));
    if(argc==2 && std::wstring(argv[1])==L"--color-depth-clear-test")return testColorClearDepthAlias();
    if(argc==2 && std::wstring(argv[1])==L"--z-color-alias-test")return testDepthClearColorAlias();
    if(argc==6 && std::wstring(argv[1])==L"--gs-sequence")return replayCompleteGs(fs::path(argv[2]),fs::path(argv[3]),_wtoi(argv[4]),false,std::max(1,_wtoi(argv[5])));
    if(argc==5 && std::wstring(argv[1])==L"--complete-gs")return replayCompleteGs(fs::path(argv[2]),fs::path(argv[3]),_wtoi(argv[4]));
    if(argc==5 && std::wstring(argv[1])==L"--complete-gs-cpu")return replayCompleteGs(fs::path(argv[2]),fs::path(argv[3]),_wtoi(argv[4]),true);
    if(argc==3 && std::wstring(argv[1])==L"--last-vu-signal")return reproduceLastVuSignal(fs::path(argv[2]));
    if(argc==2 && std::wstring(argv[1])==L"--gpu-contract")return qualifyNativeGpuContract();
    if(argc==4 && std::wstring(argv[1])==L"--benchmark-vu")return profileCapturedGraphics(fs::path(argv[2]),std::max(1,_wtoi(argv[3])),false,true);
    if(argc==4 && std::wstring(argv[1])==L"--qualify-gpu")return profileCapturedGraphics(fs::path(argv[2]),std::max(1,_wtoi(argv[3])),true);
    if(argc==4 && std::wstring(argv[1])==L"--profile-captured")return profileCapturedGraphics(fs::path(argv[2]),std::max(1,_wtoi(argv[3])));
    if(argc==4 && std::wstring(argv[1])==L"--save-test") {
        using namespace ps2x::iop;
        const fs::path originals=fs::path(argv[2]),saveRoot=fs::path(argv[3]);
        if(fs::exists(saveRoot))throw std::runtime_error("Save regression requires a new, isolated directory");
        auto runtime=std::make_unique<PS2Runtime>();
        if(!runtime->memory().initialize())throw std::runtime_error("Test RAM initialization failed");
        auto paths=PS2Runtime::getIoPaths();paths.elfDirectory=originals;paths.hostRoot=originals;paths.cdRoot=originals;paths.mcRoot=saveRoot;
        PS2Runtime::setIoPaths(paths);
        PS2IopHostAdapter host(*runtime);
        IopSubsystem storage(host);
        storage.reset();
        // Load the physical original modules too: native storage must still
        // win when original MCSERV has registered the same RPC SID.
        for(const char* name:{"SIO2MAN.IRX","PADMAN.IRX","LIBSD.IRX","SDRDRV.IRX","MCMAN.IRX","MCSERV.IRX"}) {
            const auto path=(originals/"IOP"/name).u8string();
            const auto loaded=storage.loadModule(std::string("cdrom0:\\IOP\\")+name);
            if(loaded.moduleId<0)throw std::runtime_error("Native module initialization failed");
        }
        uint8_t* ram=runtime->memory().getRDRAM();
        auto scope=host.enterCall(nullptr,ram);
        struct Descriptor {int32_t fd=0,port=0,slot=0,size=0,offset=0,origin=0;uint32_t buffer=0,parameter=0;uint8_t data[16]{};};
        struct Name {int32_t port=0,slot=0,flags=0,maxEntries=0;uint32_t pointer=0;char name[1024]{};};
        static_assert(sizeof(Descriptor)==48 && sizeof(Name)==1044);
        auto rpc=[&](uint32_t function,const auto& parameter) {
            std::memcpy(ram+0x2000,&parameter,sizeof(parameter));
            std::memset(ram+0x6000,0xcd,16);
            RpcRequest request{};request.sid=0x80000400;request.function=function;
            request.send={0x2000,static_cast<uint32_t>(sizeof(parameter))};request.receive={0x6000,16};
            const auto result=storage.handleRpc(request);
            if(!result.handled)throw std::runtime_error("Native save RPC not handled");
            int32_t value;std::memcpy(&value,ram+0x6000,4);return value;
        };
        Descriptor descriptor{};descriptor.parameter=0x5000;
        if(rpc(1,descriptor)!=0 || u32(std::vector<uint8_t>(ram+0x5000,ram+0x5100),0)!=2 || ram[0x5090]!=1)
            throw std::runtime_error("Original MCSERV still reports missing/unformatted card");
        Name name{};std::strcpy(name.name,"BASLUS-20607TEST");name.flags=0x40;
        if(rpc(2,name)!=0)throw std::runtime_error("Native save directory creation failed");
        std::strcpy(name.name,"BASLUS-20607TEST/progress.dat");name.flags=0x602;
        const int32_t fd=rpc(2,name);if(fd<0)throw std::runtime_error("Native save open failed");
        for(unsigned i=0;i<97;++i)ram[0x7003+i]=static_cast<uint8_t>(i*37u+11u);
        descriptor={};descriptor.fd=fd;descriptor.size=97;descriptor.buffer=0x7003;
        if(rpc(6,descriptor)!=97)throw std::runtime_error("Native save write failed");
        if(rpc(3,descriptor)!=0)throw std::runtime_error("Native save close failed");
        storage.reset(); // fresh process-equivalent service state; files must survive
        for(const char* module:{"SIO2MAN.IRX","PADMAN.IRX","LIBSD.IRX","SDRDRV.IRX","MCMAN.IRX","MCSERV.IRX"})if(storage.loadModule(std::string("cdrom0:\\IOP\\")+module).moduleId<0)throw std::runtime_error("Reload failed");
        name.flags=1;const int32_t readFd=rpc(2,name);
        if(readFd<0)throw std::runtime_error("Save disappeared after storage reset");
        descriptor={};descriptor.fd=readFd;descriptor.size=97;descriptor.buffer=0x8003;
        if(rpc(5,descriptor)!=97 || std::memcmp(ram+0x7003,ram+0x8003,97))throw std::runtime_error("Save readback differs from original bytes");
        if(rpc(3,descriptor)!=0)throw std::runtime_error("Native save read close failed");
        name={};std::strcpy(name.name,"BASLUS-20607TEST/*");name.maxEntries=4;name.pointer=0x9000;
        const int32_t entries=rpc(0x0d,name);
        bool foundSave=false;
        if(entries>0 && entries<=4)for(int32_t i=0;i<entries;++i)
            foundSave|=std::strcmp(reinterpret_cast<char*>(ram+0x9020+i*64),"progress.dat")==0;
        if(!foundSave)throw std::runtime_error("GetDir did not return the persisted save entry through the sixth ABI argument");
        if(fs::file_size(saveRoot/"BASLUS-20607TEST"/"progress.dat")!=97)throw std::runtime_error("Windows save file size incorrect");
        std::cout<<"PASS: original MCSERV present; PC storage wins; formatted card; create/write/close/reset/read; unaligned payload; GetDir sixth argument; persisted Windows file\n";
        return 0;
    }
    if(argc==2 && std::wstring(argv[1])==L"--presentation-test") {
        std::vector<uint8_t> vram(4u*1024u*1024u);
        auto backend=std::make_unique<GSCpuBackend>();backend->Initialize(vram.data(),static_cast<uint32_t>(vram.size()));
        for(uint32_t y=0;y<64;++y)for(uint32_t x=0;x<64;++x) {
            const uint32_t pixel=(y&1u)?0xffc80000u:0xff000064u;
            backend->WriteVram(0,0,1,x,y,pixel);
            backend->WriteVram(0,256,1,x,y,pixel);
        }
        GSPresentationRequest request{};
        request.pmode=0x8027u;request.smode2=1u;
        request.dispfb1=request.dispfb2=(1ull<<9);
        request.display1=request.display2=(63ull<<32)|(63ull<<44);
        const auto even=backend->Present(request);
        request.vsyncTick=1;
        const auto odd=backend->Present(request);
        if(!even || even.width!=64 || even.height!=64 || even.pixels!=odd.pixels)
            throw std::runtime_error("Stationary interlaced image jitters across field parity");
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x) {
            const auto* pixel=even.pixels.data()+(y*640u+x)*4u;
            if(pixel[0]!=50 || pixel[1]!=0 || pixel[2]!=100 || pixel[3]!=255)
                throw std::runtime_error("Field blend has incorrect RGB/alpha or row stride");
        }
        // Different base with identical contents forces the generic dual-CRT
        // path. It must match the optimized identical-frame path byte for byte.
        request.dispfb2|=8u;
        const auto generic=backend->Present(request);
        if(generic.pixels!=even.pixels)throw std::runtime_error("Dual-output identity optimization changes pixels");
        request.smode2=3u;request.dispfb2=request.dispfb1;
        const auto progressive=backend->Present(request);
        if(progressive.pixels[0]!=100 || progressive.pixels[640u*4u+2]!=200)
            throw std::runtime_error("Progressive frame was incorrectly deinterlaced");
        std::cout<<"PASS: stable even/odd field blend; RGB and alpha; dual-output fast path equals generic composite; progressive detail retained\n";
        return 0;
    }
    if(argc==2 && std::wstring(argv[1])==L"--pad-test") {
        std::vector<uint8_t> ram(32u*1024*1024);auto c=std::make_unique<R5900Context>();
        ps2_stubs::scePadInit(ram.data(),c.get(),nullptr);
        SET_GPR_U32(c.get(),4,0);SET_GPR_U32(c.get(),5,0);SET_GPR_U32(c.get(),6,0x1000);
        ps2_stubs::scePadPortOpen(ram.data(),c.get(),nullptr);
        SET_GPR_U32(c.get(),6,1);ps2_stubs::scePadSetMainMode(ram.data(),c.get(),nullptr);
        ps2_stubs::scePadEnterPressMode(ram.data(),c.get(),nullptr);
        for(unsigned i=0;i<16;++i) {
            const uint16_t buttons=static_cast<uint16_t>(0xffffu^(1u<<i));
            ps2_stubs::setPadOverrideState(buttons,1,254,12,243);
            SET_GPR_U32(c.get(),6,0x2000);ps2_stubs::scePadRead(ram.data(),c.get(),nullptr);
            const auto *data=ram.data()+0x2000;
            const uint16_t observed=static_cast<uint16_t>(data[2]|(data[3]<<8));
            if(observed!=buttons || data[6]!=1 || data[7]!=254 || data[4]!=12 || data[5]!=243 || data[14]!=(i==14?255:0))
                throw std::runtime_error("Disney pad packet swapped action/directional bits for bit "+std::to_string(i));
        }
        ps2_stubs::setPadOverrideState(0xffff,128,128,128,128);
        ps2_stubs::scePadRead(ram.data(),c.get(),nullptr);
        if(ram[0x2002]!=255 || ram[0x2003]!=255)throw std::runtime_error("Pad release stuck");
        ps2_stubs::clearPadOverrideState();
        std::cout<<"PASS: all 16 buttons preserve the libpad little-endian field; analog axes, cross pressure and release work\n";return 0;
    }
    if(argc==3 && (std::wstring(argv[1])==L"--job" || std::wstring(argv[1])==L"--step-job" || std::wstring(argv[1])==L"--check-disney-job" || std::wstring(argv[1])==L"--check-image2-job")) {
        std::ifstream f(fs::path(argv[2]),std::ios::binary);std::array<uint32_t,4> h{};
        f.read(reinterpret_cast<char*>(h.data()),sizeof(h));
        if(h[3]!=sizeof(VU1State))throw std::runtime_error("Snapshot VU ABI mismatch");
        auto state=std::make_unique<VU1State>();f.read(reinterpret_cast<char*>(state.get()),sizeof(*state));
        auto m=std::make_unique<PS2Memory>();if(!m->initialize())return 1;
        f.read(reinterpret_cast<char*>(m->getVU1Code()),16384);f.read(reinterpret_cast<char*>(m->getVU1Data()),16384);
        if(!f)throw std::runtime_error("Short native VU snapshot");
        auto g=std::make_unique<GS>();g->init(m->getGSVRAM(),4u*1024*1024,&m->gs());
        unsigned packets=0;bool image2Terminator=false;
        m->setGifPacketCallback([&](const uint8_t*p,uint32_t n){++packets;uint64_t lo,hi;std::memcpy(&lo,p,8);std::memcpy(&hi,p+8,8);if((lo&0xffffu)==0x8000u && ((lo>>58)&3u)==3u)image2Terminator=true;std::cout<<"packet="<<packets<<" bytes="<<n<<" tag="<<std::hex<<lo<<","<<hi<<std::dec<<'\n';g->processGIFPacket(p,n);});
        auto vu=std::make_unique<VU1Interpreter>();vu->state()=*state;
        const unsigned callerRounding=_mm_getcsr()&0x6000u;
        const bool single=std::wstring(argv[1])==L"--step-job";
        vu->execute(m->getVU1Code(),16384,m->getVU1Data(),16384,*g,m.get(),h[0],h[1],h[2],single?1:1000000);
        if(single) {
            uint64_t previous[2]{};std::memcpy(previous,m->getVU1Data()+0x2e70,16);
            for(unsigned steps=0;steps<10000;++steps) {
                const auto beforePc=vu->state().pc;const bool ending=vu->state().ebit || vu->state().haltAfterDelaySlot;
                vu->resume(m->getVU1Code(),16384,m->getVU1Data(),16384,*g,m.get(),h[1],h[2],1);
                uint64_t now[2];std::memcpy(now,m->getVU1Data()+0x2e70,16);
                if(std::memcmp(now,previous,16)) {
                    std::cout<<"header-write pc="<<std::hex<<beforePc<<" tag="<<now[0]<<","<<now[1]<<std::dec<<'\n';std::memcpy(previous,now,16);
                }
                if(ending && !vu->state().ebit && !vu->state().haltAfterDelaySlot){std::cout<<"Native ending after steps="<<steps<<'\n';break;}
            }
        }
        if(std::wstring(argv[1])==L"--check-image2-job") {
            if(vu->state().pc!=0x338u || vu->state().vi[2]!=0x31 || !image2Terminator || (_mm_getcsr()&0x6000u)!=callerRounding)
                throw std::runtime_error("Original Disney empty IMAGE2 termination regression failed");
            std::cout<<"PASS: original Disney clipped geometry completes across an empty IMAGE2/EOP tag without a false reserved-instruction stop\n";
        }
        if(std::wstring(argv[1])==L"--check-disney-job") {
            uint32_t header;std::memcpy(&header,m->getVU1Data()+0x2e74,4);
            if(vu->state().pc!=0x338u || packets!=7 || header!=0x302ec040u || (_mm_getcsr()&0x6000u)!=callerRounding)
                throw std::runtime_error("Original Disney VU1 header/termination/rounding regression failed");
            std::cout<<"PASS: original Disney job emits seven complete tags, correct NREG/header, native E-bit termination and restored host rounding\n";
        }
        std::cout<<"Native single job pc="<<std::hex<<h[0]<<" -> "<<vu->state().pc<<" vi2="<<vu->state().vi[2]<<" vi4="<<vu->state().vi[4]<<" vi5="<<vu->state().vi[5]<<" vi6="<<vu->state().vi[6]<<" CSR="<<m->gs().csr.load()<<std::dec<<" packets="<<packets<<'\n';return 0;
    }
    if(argc!=2)throw std::runtime_error("Usage: DisneyOriginalGraphicsReplay <retained analysis directory>");
    const fs::path directory(argv[1]);
    const auto ram=read(directory/"render-wait.ram",32u*1024*1024);
    const auto code=read(directory/"render-vu1-code.bin",16384);
    const auto data=read(directory/"render-vu1-data.bin",16384);
    std::cout<<"Native state sizes: memory="<<sizeof(PS2Memory)<<" VU="<<sizeof(VU1Interpreter)<<" runtime="<<sizeof(PS2Runtime)<<'\n';
    auto memoryStorage=std::make_unique<PS2Memory>();auto &memory=*memoryStorage;
    if(!memory.initialize())throw std::runtime_error("Cannot allocate native test memory");
    std::memcpy(memory.getRDRAM(),ram.data(),ram.size());
    std::memcpy(memory.getVU1Code(),code.data(),code.size());
    std::memcpy(memory.getVU1Data(),data.data(),data.size());
    auto graphicsStorage=std::make_unique<GS>();auto &graphics=*graphicsStorage;
    graphics.init(memory.getGSVRAM(),4u*1024*1024,&memory.gs());
    unsigned packets=0,jobs=0;
    memory.setGifPacketCallback([&](const uint8_t*p,uint32_t n){++packets;graphics.processGIFPacket(p,n);});
    auto nativeStorage=std::make_unique<VU1Interpreter>();auto &native=*nativeStorage;
    memory.setVu1MscalCallback([&](uint32_t pc,uint32_t top,uint32_t itop){
        ++jobs;std::cout<<"Native VU job pc=0x"<<std::hex<<pc<<std::dec<<'\n';
        native.execute(memory.getVU1Code(),16384,memory.getVU1Data(),16384,graphics,&memory,pc,top,itop,1000000);
        std::cout<<"Native VU return pc=0x"<<std::hex<<native.state().pc<<std::dec<<" cycles="<<native.state().cycles<<'\n';
    });
    memory.setVu1MscntCallback([&](uint32_t top,uint32_t itop){
        ++jobs;native.resume(memory.getVU1Code(),16384,memory.getVU1Data(),16384,graphics,&memory,top,itop,1000000);
    });
    // First global setup chain, then its corresponding final busy-node chain.
    // These are a bounded historical replay, not restoration of all device state.
    for(uint32_t pointer:{0x3676f8u,0xc275d0u+0x18u}) {
        const auto bytes=chain(ram,u32(ram,pointer));
        std::cout<<"Retained stream bytes="<<bytes.size()<<'\n';
        memory.processVIF1Data(bytes.data(),static_cast<uint32_t>(bytes.size()));
    }
    std::cout<<"Replay native jobs="<<jobs<<" GIF packets="<<packets<<" CSR=0x"<<std::hex<<memory.gs().csr.load()<<std::dec<<'\n';
    std::cout<<"No visual-equivalence claim: original VRAM, GS registers and initial VU registers are incomplete\n";
    return 0;
 }catch(const std::exception&e){std::cerr<<"Native replay stopped: "<<e.what()<<'\n';return 1;}
}
