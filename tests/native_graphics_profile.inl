// Offline profiling of retained original inputs: never boots the game.
#include <chrono>
#include <iomanip>
#include <map>
#include "../native/original/graphics/gpu/gs_hw_backend.h"
std::unique_ptr<GSRasterBackend> makeDisneyNativeGpuBackend();
struct NativeProfileBackend final : GSRasterBackend {
    std::unique_ptr<GSRasterBackend> inner;
    bool discard=false;
    uint64_t draws=0,rasterNs=0;
    std::array<uint64_t,8> typeCount{},typeNs{};
    explicit NativeProfileBackend(bool drop,unsigned gpu=0):discard(drop),inner(gpu==2?makeDisneyNativeGpuBackend():gpu==1?std::unique_ptr<GSRasterBackend>(new GSHwBackend):std::unique_ptr<GSRasterBackend>(new GSCpuBackend)){}
    void Initialize(uint8_t* vram,uint32_t size) override {inner->Initialize(vram,size);}
    void Reset() override {inner->Reset();}
    void LoadClut(const GSTex0Reg& tex,const GSTexClutReg& clut) override {inner->LoadClut(tex,clut);}
    void BeginTransfer(const GSTransferCommand& command) override {inner->BeginTransfer(command);}
    void UploadImage(const uint8_t* data,uint32_t size) override {inner->UploadImage(data,size);}
    void Flush() override {inner->Flush();}
    void TextureFlush() override {inner->TextureFlush();}
    void Sync(GSSyncReason reason) override {inner->Sync(reason);}
    PresentationFrame Present(const GSPresentationRequest& request) override {return inner->Present(request);}
    bool ClearFramebuffer(const GSContext& context,uint32_t rgba) override {return inner->ClearFramebuffer(context,rgba);}
    uint32_t ConsumeLocalToHostBytes(uint8_t* dst,uint32_t size) override {return inner->ConsumeLocalToHostBytes(dst,size);}
    uint32_t ReadVram(uint32_t psm,uint32_t base,uint32_t bw,uint32_t x,uint32_t y) const override {return inner->ReadVram(psm,base,bw,x,y);}
    void WriteVram(uint32_t psm,uint32_t base,uint32_t bw,uint32_t x,uint32_t y,uint32_t value) override {inner->WriteVram(psm,base,bw,x,y,value);}
    void SnapshotVram(std::vector<uint8_t>& out) const override {inner->SnapshotVram(out);}
    GSTransferSnapshot GetTransferSnapshot() const override {return inner->GetTransferSnapshot();}
    void Submit(const GSPrimitiveBatch& batch) override {
        const auto begin=std::chrono::steady_clock::now();
        if(!discard)inner->Submit(batch);
        const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count();
        const unsigned kind=static_cast<unsigned>(batch.state.prim.type)&7u;
        ++draws;rasterNs+=ns;++typeCount[kind];typeNs[kind]+=ns;
    }
};
struct NativeCapturedJob {
    std::array<uint32_t,4> header;
    VU1State state;
    std::array<uint8_t,16384> code,data;
};
int profileCapturedGraphics(const fs::path& directory,unsigned repeats,bool qualifyGpu=false,bool skipCpuRaster=false) {
    std::vector<fs::path> paths;
    for(const auto& entry:fs::directory_iterator(directory))
        if(entry.path().filename().wstring().rfind(L"native-vu-job-ring-",0)==0 && entry.path().extension()==L".bin")paths.push_back(entry.path());
    if(paths.empty())throw std::runtime_error("No retained original VU jobs");
    std::sort(paths.begin(),paths.end(),[](const auto&a,const auto&b){const auto at=fs::last_write_time(a),bt=fs::last_write_time(b);return at!=bt?at<bt:a<b;});
    std::vector<NativeCapturedJob> jobs;
    for(const auto& path:paths) {
        NativeCapturedJob job{};std::ifstream f(path,std::ios::binary);
        f.read(reinterpret_cast<char*>(job.header.data()),16);
        if(job.header[3]!=sizeof(VU1State))throw std::runtime_error("Retained VU state ABI differs");
        f.read(reinterpret_cast<char*>(&job.state),sizeof(job.state));
        f.read(reinterpret_cast<char*>(job.code.data()),job.code.size());
        f.read(reinterpret_cast<char*>(job.data.data()),job.data.size());
        if(!f)throw std::runtime_error("Truncated retained VU input");jobs.push_back(job);
    }
    const auto initialVram=read(directory/"native-aot-vram.bin",4u*1024u*1024u);
    PresentationFrame referenceFrame,referenceTarget,referenceGpu;
    uint64_t referenceHash=0,referencePackets=0,referenceCycles=0;
    std::vector<VU1State> finalStates;
    std::cout<<"Captured inputs="<<jobs.size()<<" repeats="<<repeats<<"; independent jobs, not a restored whole game frame\n";
    for(unsigned mode=0;mode<(qualifyGpu?5u:3u);++mode) {
        auto memory=std::make_unique<PS2Memory>();if(!memory->initialize())throw std::runtime_error("Offline memory allocation failed");
        std::memcpy(memory->getGSVRAM(),initialVram.data(),initialVram.size());
        auto gs=std::make_unique<GS>();gs->init(memory->getGSVRAM(),initialVram.size(),&memory->gs());gs->setDebugHistoryPaused(true);
        auto backend=std::make_unique<NativeProfileBackend>((mode==0 && skipCpuRaster) || mode==1 || mode==2,mode>=3?mode-2:0);auto* metrics=backend.get();gs->setRasterBackend(std::move(backend));
        auto vu=std::make_unique<VU1Interpreter>();
        uint64_t packets=0,bytes=0,hash=0xcbf29ce484222325ull,cycles=0,codeChanges=0,codeReuses=0;
        uint64_t finalStateHash=0xcbf29ce484222325ull, finalDataHash=0xcbf29ce484222325ull;
        memory->setGifPacketCallback([&](const uint8_t* data,uint32_t size){
            ++packets;bytes+=size;for(uint32_t i=0;i<size;++i){hash^=data[i];hash*=0x100000001b3ull;}
            if(mode!=2)gs->processGIFPacket(data,size);
        });
        const auto begin=std::chrono::steady_clock::now();
        for(unsigned repeat=0;repeat<repeats;++repeat)for(const auto& job:jobs) {
            if(std::memcmp(memory->getVU1Code(),job.code.data(),16384)!=0) {
                std::memcpy(memory->getVU1Code(),job.code.data(),16384);memory->markVU1CodeModified();++codeChanges;
            } else ++codeReuses;
            std::memcpy(memory->getVU1Data(),job.data.data(),16384);
            vu->state()=job.state;
            vu->execute(memory->getVU1Code(),16384,memory->getVU1Data(),16384,*gs,memory.get(),job.header[0],job.header[1],job.header[2],1000000u);
            cycles+=vu->state().cycles;
            auto foldBytes=[](uint64_t& h,const uint8_t* ptr,size_t n){for(size_t i=0;i<n;++i){h^=ptr[i];h*=0x100000001b3ull;}};
            foldBytes(finalStateHash,reinterpret_cast<const uint8_t*>(&vu->state()),sizeof(VU1State));
            foldBytes(finalDataHash,memory->getVU1Data(),16384);
            if(mode==0)finalStates.push_back(vu->state());
            else {
                const auto& expected=finalStates[repeat*jobs.size()+(&job-jobs.data())];
                if(std::memcmp(&expected,&vu->state(),sizeof(VU1State))!=0)
                    throw std::runtime_error("Profiling path changed original final VU state");
            }
        }
        metrics->Sync(GSSyncReason::DebugReadback);
        const double elapsedMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        if(mode==0){referenceHash=hash;referencePackets=packets;referenceCycles=cycles;}
        else if(hash!=referenceHash || packets!=referencePackets || cycles!=referenceCycles)
            throw std::runtime_error("Profiling paths changed original VU output bytes/cycles");
        std::cout<<std::fixed<<std::setprecision(3)<<"mode="<<(mode==0?(skipCpuRaster?"native-vu+frontend-reference":"native-vu+cpu-raster"):mode==1?"native-vu+frontend-no-raster":mode==2?"native-vu-gif-only":mode==3?"native-vu+native-gpu":"native-vu+gpu-worker")
            <<" elapsedMs="<<elapsedMs<<" rasterMs="<<metrics->rasterNs/1e6<<" jobs="<<jobs.size()*repeats
            <<" draws="<<metrics->draws<<" packets="<<packets<<" bytes="<<bytes<<" outputHash="<<std::hex<<hash<<" finalStateHash="<<finalStateHash<<" finalDataHash="<<finalDataHash<<std::dec
            <<" cycles="<<cycles<<" codeChanges="<<codeChanges<<" codeReuses="<<codeReuses<<"\n";
        if((mode==0 && !skipCpuRaster) || mode>=3) {
            GSPresentationRequest request{};
            request.pmode=0x8027u;request.smode2=0x110001u;
            request.dispfb1=request.dispfb2=0x45148cu;
            request.display1=0x1bf9ff0183127cull;request.display2=0x1bf9ff0183227cull;
            const auto frame=metrics->Present(request);
            std::cout<<"displayed="<<frame.width<<"x"<<frame.height<<" storageBytes="<<frame.pixels.size()<<"\n";
            if(mode==3)referenceGpu=frame;
            if(mode==4 && (frame.width!=referenceGpu.width || frame.height!=referenceGpu.height || frame.pixels!=referenceGpu.pixels))
                throw std::runtime_error("GPU worker changed ordered draw/upload/presentation results");
            // Compare only visible RGB: CPU's storage is fixed 640x512 while its active height is 448.
            if(mode==0)referenceFrame=frame;
            else if(frame && referenceFrame && frame.width==referenceFrame.width && frame.height==referenceFrame.height) {
                const size_t referenceStride=referenceFrame.pixels.size()/512u;
                size_t differences=0;uint64_t error=0;
                for(unsigned y=0;y<frame.height;++y)for(unsigned x=0;x<frame.width;++x) {
                    bool changed=false;
                    for(unsigned c=0;c<3;++c){const int a=frame.pixels[y*frame.width*4u+x*4u+c],b=referenceFrame.pixels[y*referenceStride+x*4u+c];changed|=a!=b;error+=std::abs(a-b);}
                    differences+=changed;
                }
                std::cout<<"GPU qualification differing RGB pixels="<<differences<<" / "<<frame.width*frame.height<<" meanChannelError="<<double(error)/(frame.width*frame.height*3u)<<"; incomplete-frame replay, not gameplay validation\n";
            }
            if(frame) {
                std::ofstream ppm(directory/(mode==0?"native-offline-cpu.ppm":"native-offline-gpu.ppm"),std::ios::binary);
                ppm<<"P6\n"<<frame.width<<" "<<frame.height<<"\n255\n";
                const size_t row=mode==0?640u*4u:frame.width*4u;
                for(unsigned y=0;y<frame.height;++y)for(unsigned x=0;x<frame.width;++x)ppm.write(reinterpret_cast<const char*>(frame.pixels.data()+y*row+x*4u),3);
            }
            // Captures do not contain PATH3 final buffer-copy commands. Compare the
            // actual draw target separately, instead of pretending this is a whole frame.
            request.pmode=1;request.smode2=3;request.dispfb1=10ull<<9;
            const auto target=metrics->Present(request);
            if(mode==0)referenceTarget=target;
            else if(target && referenceTarget) {
                size_t different=0;uint64_t error=0;
                for(unsigned y=2;y<448;++y)for(unsigned x=2;x<640;++x) {
                    bool changed=false;
                    for(unsigned c=0;c<3;++c){const int a=target.pixels[y*2560u+x*4u+c],b=referenceTarget.pixels[y*2560u+x*4u+c];changed|=a!=b;error+=std::abs(a-b);}
                    different+=changed;
                }
                std::cout<<"DRAW-TARGET comparison differing pixels="<<different<<" meanChannelError="<<double(error)/(638u*446u*3u)<<"\n";
            }
            if(target) {
                std::ofstream ppm(directory/(mode==0?"native-offline-cpu-target.ppm":"native-offline-gpu-target.ppm"),std::ios::binary);
                ppm<<"P6\n640 448\n255\n";
                for(unsigned y=0;y<448;++y)for(unsigned x=0;x<640;++x)ppm.write(reinterpret_cast<const char*>(target.pixels.data()+y*2560u+x*4u),3);
            }
            if(mode==0) {
                std::vector<uint8_t> vram;metrics->SnapshotVram(vram);uint64_t h=0xcbf29ce484222325ull;
                for(uint8_t b:vram){h^=b;h*=0x100000001b3ull;}
                std::cout<<"CPU final full-VRAM hash="<<std::hex<<h<<std::dec<<" bytes="<<vram.size()<<"\n";
            }
        }
        if(mode==0)for(unsigned type=0;type<8;++type)if(metrics->typeCount[type])
            std::cout<<"primitive="<<type<<" draws="<<metrics->typeCount[type]<<" rasterMs="<<metrics->typeNs[type]/1e6<<"\n";
    }
    std::cout<<"PASS: offline paths emit identical original GIF bytes, packet counts, VU cycles and final VU states; no game launched\n";
    if(qualifyGpu)std::cout<<"PASS: GPU worker matches direct GPU image exactly; CPU/GPU compatibility is reported separately\n";
    return 0;
}

