int testColorClearDepthAlias(){
 std::vector<uint8_t> cm(4u*1024u*1024u),gm(cm.size());GSCpuBackend cpu;GSHwBackend gpu;
 cpu.Initialize(cm.data(),static_cast<uint32_t>(cm.size()));gpu.Initialize(gm.data(),static_cast<uint32_t>(gm.size()));
 GSPrimitiveBatch b{};b.vertexCount=2;auto& s=b.state;auto& c=s.context;
 s.prim.type=GS_PRIM_SPRITE;s.prim.fst=true;s.colclamp=1;c.frame.fbw=1;c.scissor.x1=c.scissor.y1=63;
 c.zbuf.zbp=3;c.zbuf.psm=GS_PSM_Z24;c.test=0x50000;b.vertices[1].x=b.vertices[1].y=64;
 for(auto& v:b.vertices){v.r=200;v.g=30;v.b=20;v.a=128;v.z=0x800000;v.q=1;}
 auto submit=[&]{cpu.Submit(b);gpu.Submit(b);cpu.Flush();gpu.Flush();};submit();
 // Clear the depth memory using the original paired CT32/Z24 operation.
 c.frame.fbp=3;c.frame.fbmsk=0xff000000;c.zbuf.zbp=4;c.test=0x30000;b.vertices[1].y=32;
 for(auto& v:b.vertices){v.r=v.g=v.b=v.a=0;v.z=0;}submit();
 // The following color clear uses another depth alias, just like SLUS_206.07.
 c.frame.fbp=0;c.zbuf.zbp=1;c.zbuf.psm=GS_PSM_Z32;
 for(auto& v:b.vertices){v.r=80;v.g=96;v.b=112;v.z=0x706050;}submit();
 c.frame.fbmsk=0;c.zbuf.zbp=3;c.zbuf.psm=GS_PSM_Z24;c.test=0x50000;b.vertices[1].y=64;
 for(auto& v:b.vertices){v.r=20;v.g=220;v.b=40;v.a=128;v.z=0x100000;}submit();
 GSPresentationRequest r{};r.pmode=1;r.smode2=3;r.dispfb1=1ull<<9;r.display1=(63ull<<32)|(63ull<<44);
 auto a=cpu.Present(r),g=gpu.Present(r);unsigned bad=0,oracleBad=0;
 const unsigned expected[3]={20,220,40};
 for(unsigned y=2;y<64;++y)for(unsigned x=2;x<64;++x){unsigned i=(y*640+x)*4u;for(unsigned ch=0;ch<3;++ch){oracleBad+=a.pixels[i+ch]!=expected[ch];bad+=g.pixels[i+ch]!=a.pixels[i+ch];}}
 std::cout<<"CT32/Z24 depth clear: CPU oracle differences="<<oracleBad<<" GPU differing channels="<<bad<<"; geometry behind previous frame depth must become visible\n";
 return bad||oracleBad?1:0;
}
