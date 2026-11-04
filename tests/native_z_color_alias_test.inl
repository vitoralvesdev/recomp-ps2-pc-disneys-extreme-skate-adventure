int testDepthClearColorAlias(){
 std::vector<uint8_t> cm(4u*1024u*1024u),gm(cm.size());GSCpuBackend cpu;GSHwBackend gpu;
 cpu.Initialize(cm.data(),static_cast<uint32_t>(cm.size()));gpu.Initialize(gm.data(),static_cast<uint32_t>(gm.size()));
 GSPrimitiveBatch b{};b.vertexCount=2;auto& s=b.state;auto& c=s.context;
 s.prim.type=GS_PRIM_SPRITE;s.prim.fst=true;s.colclamp=1;c.frame.fbw=1;c.scissor.x1=c.scissor.y1=63;c.zbuf.zmask=true;c.test=0x30000;
 b.vertices[1].x=b.vertices[1].y=64;for(auto& v:b.vertices){v.r=200;v.g=30;v.b=20;v.a=128;v.q=1;}
 cpu.Submit(b);gpu.Submit(b);cpu.Flush();gpu.Flush();
 // Real captured clear: half-height color write and Z32 write into the other half.
 c.frame.fbmsk=0xff000000;c.zbuf.zmask=false;c.zbuf.zbp=1;c.zbuf.psm=GS_PSM_Z32;b.vertices[1].y=32;
 for(auto& v:b.vertices){v.r=80;v.g=96;v.b=112;v.a=0;v.z=0x706050;}
 cpu.Submit(b);gpu.Submit(b);
 GSPresentationRequest r{};r.pmode=1;r.smode2=3;r.dispfb1=1ull<<9;r.display1=(63ull<<32)|(63ull<<44);
 auto a=cpu.Present(r),g=gpu.Present(r);unsigned bad=0,oracleBad=0;
 for(unsigned y=2;y<64;++y)for(unsigned x=2;x<64;++x){unsigned i=(y*640+x)*4u;for(unsigned ch=0;ch<3;++ch){unsigned expected=80+ch*16;oracleBad+=a.pixels[i+ch]!=expected;bad+=g.pixels[i+ch]!=a.pixels[i+ch];}}
 std::cout<<"Z32/color alias: CPU oracle differences="<<oracleBad<<" GPU differing channels="<<bad<<" lowerPixel CPU="<<unsigned(a.pixels[(48u*640u+32u)*4u])<<","<<unsigned(a.pixels[(48u*640u+32u)*4u+1])<<","<<unsigned(a.pixels[(48u*640u+32u)*4u+2])<<" GPU="<<unsigned(g.pixels[(48u*640u+32u)*4u])<<","<<unsigned(g.pixels[(48u*640u+32u)*4u+1])<<","<<unsigned(g.pixels[(48u*640u+32u)*4u+2])<<"\n";
 return bad||oracleBad?1:0;
}
