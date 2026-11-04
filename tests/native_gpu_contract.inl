// Complete synthetic inputs, unlike the historical partial game captures.
int qualifyNativeGpuContract() {
    const char* names[]={"flat sprite","point RGBA texture","linear RGBA texture","indexed PSMT8 texture","framebuffer alpha bit"};
    unsigned failures=0;
    for(unsigned test=0;test<5;++test) {
        std::vector<uint8_t> cpuMemory(4u*1024u*1024u),gpuMemory(cpuMemory.size());
        GSCpuBackend cpu;GSHwBackend gpu;
        cpu.Initialize(cpuMemory.data(),static_cast<uint32_t>(cpuMemory.size()));gpu.Initialize(gpuMemory.data(),static_cast<uint32_t>(gpuMemory.size()));
        GSPrimitiveBatch batch{};batch.vertexCount=2;
        auto& s=batch.state;auto& c=s.context;
        s.prim.type=static_cast<GSPrimType>(6);s.prim.fst=true;s.colclamp=1;s.textureWidth=s.textureHeight=8;
        c.frame.fbw=1;c.scissor.x1=c.scissor.y1=63;c.zbuf.zmask=true;c.test=0x30000;
        c.tex0.tbp0=0x1000;c.tex0.tbw=1;c.tex0.tw=c.tex0.th=3;c.tex0.tcc=1;c.tex0.tfx=1;
        s.texa.ta0=s.texa.ta1=128;
        batch.vertices[0].x=batch.vertices[0].y=8;
        batch.vertices[1].x=batch.vertices[1].y=56;batch.vertices[1].u=batch.vertices[1].v=8u<<4;
        for(auto& v:batch.vertices){v.r=100;v.g=150;v.b=200;v.a=128;v.q=1;}
        if(test>=1 && test<=3) {
            s.prim.tme=true;s.linearFilter=test==2;
            for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x) {
                const uint32_t color=0x80000000u|(x*32u)|((y*32u)<<8)|(80u<<16);
                for(GSRasterBackend* backend:{static_cast<GSRasterBackend*>(&cpu),static_cast<GSRasterBackend*>(&gpu)})
                    backend->WriteVram(0,0x1000,1,x,y,color);
            }
        }
        if(test==3) {
            c.tex0.psm=0x13;c.tex0.cbp=0x1800;c.tex0.cld=1;c.tex0.csm=1;
            s.texclut.cbw=1;
            for(GSRasterBackend* backend:{static_cast<GSRasterBackend*>(&cpu),static_cast<GSRasterBackend*>(&gpu)}) {
                for(unsigned x=0;x<256;++x)backend->WriteVram(0,0x1800,1,x,0,0x80400000u|(x&255u)|(((255u-x)&255u)<<8));
                for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x)backend->WriteVram(0x13,0x1000,1,x,y,x+y*8u);
                backend->LoadClut(c.tex0,s.texclut);
            }
        }
        if(test==4){c.fba=1;for(auto& v:batch.vertices)v.a=0;}
        cpu.Submit(batch);gpu.Submit(batch);
        if(test==4) {
            // Observe stored bit 7 through a destination-alpha-tested second draw.
            c.fba=0;c.test|=(1ull<<14)|(1ull<<15);
            for(auto& v:batch.vertices){v.r=20;v.g=240;v.b=40;v.a=128;}
            cpu.Submit(batch);gpu.Submit(batch);
        }
        GSPresentationRequest request{};request.pmode=1;request.smode2=3;request.dispfb1=1ull<<9;request.display1=(63ull<<32)|(63ull<<44);
        const auto a=cpu.Present(request),b=gpu.Present(request);
        if(!a || !b || a.width!=64 || b.width!=64 || a.height!=64 || b.height!=64)throw std::runtime_error("Native GPU contract: invalid frame");
        size_t changed=0;unsigned maxError=0;
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x) {
            bool different=false;for(unsigned channel=0;channel<3;++channel) {
                const unsigned at=(y*640u+x)*4u+channel;
                const unsigned delta=static_cast<unsigned>(std::abs(int(a.pixels[at])-int(b.pixels[at])));
                different|=delta!=0;maxError=std::max(maxError,delta);
            }
            changed+=different;
        }
        std::cout<<"GPU contract "<<names[test]<<": differing RGB pixels="<<changed<<" maxChannelError="<<maxError<<"\n";
        failures+=changed!=0;
    }
    std::cout<<(failures?"FAIL":"PASS")<<": native GPU contract failures="<<failures<<"; game not launched\n";
    return failures?1:0;
}
