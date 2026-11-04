int testSharedDepthAcrossColorTargets(){
 std::vector<uint8_t> cm(4u*1024u*1024u),gm(cm.size());GSCpuBackend cpu;GSHwBackend gpu;
 cpu.Initialize(cm.data(),static_cast<uint32_t>(cm.size()));gpu.Initialize(gm.data(),static_cast<uint32_t>(gm.size()));
 GSPrimitiveBatch b{};b.vertexCount=2;auto& s=b.state;auto& c=s.context;
 s.prim.type=GS_PRIM_SPRITE;s.prim.fst=true;s.colclamp=1;c.frame.fbw=1;c.scissor.x1=c.scissor.y1=63;c.zbuf.zbp=3;c.zbuf.psm=GS_PSM_Z24;c.test=0x50000;b.vertices[1].x=b.vertices[1].y=64;
 for(auto& v:b.vertices){v.r=200;v.g=30;v.b=20;v.a=128;v.z=0x800000;v.q=1;}
 auto submit=[&]{cpu.Submit(b);gpu.Submit(b);cpu.Flush();gpu.Flush();};submit();
 // The display pass writes Z=0 to the SAME ZBP used by the scene color target.
 c.frame.fbp=10;c.test=0x30000;for(auto& v:b.vertices){v.r=v.g=v.b=0;v.z=0;}submit();
 c.frame.fbp=0;c.test=0x50000;for(auto& v:b.vertices){v.r=20;v.g=220;v.b=40;v.z=0x100000;}submit();
 GSPresentationRequest r{};r.pmode=1;r.smode2=3;r.dispfb1=1ull<<9;r.display1=(63ull<<32)|(63ull<<44);auto a=cpu.Present(r),g=gpu.Present(r);
 unsigned bad=0,oracleBad=0;const unsigned expected[3]={20,220,40};
 for(unsigned y=2;y<64;++y)for(unsigned x=2;x<64;++x){unsigned i=(y*640+x)*4u;for(unsigned ch=0;ch<3;++ch){oracleBad+=a.pixels[i+ch]!=expected[ch];bad+=g.pixels[i+ch]!=a.pixels[i+ch];}}
 std::cout<<"Shared ZBP across color targets: CPU oracle differences="<<oracleBad<<" GPU differing channels="<<bad<<"\n";return bad||oracleBad?1:0;
}
