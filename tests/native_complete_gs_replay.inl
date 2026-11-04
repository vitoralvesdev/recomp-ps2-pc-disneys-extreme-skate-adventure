// Replays an ordered, same-build GS command capture without booting the game.
#include "../native/original/graphics/gpu/native_gpu_capture.h"
#include <map>
#include <raylib.h>
template<class T> T capturedValue(const std::vector<uint8_t>& bytes){
 if(bytes.size()!=sizeof(T))throw std::runtime_error("Capture record ABI mismatch");
 T value;std::memcpy(&value,bytes.data(),sizeof(T));return value;
}
int replayCompleteGs(const fs::path& source,const fs::path& destination,unsigned stopTick,bool cpuOnly=false,unsigned sequenceCount=0){
 std::ifstream file(source,std::ios::binary);DisneyCapture::Header h{},expected{};
 file.read(reinterpret_cast<char*>(&h),sizeof(h));
 if(!file || std::memcmp(&h,&expected,sizeof(h)))throw std::runtime_error("Capture header ABI mismatch");
 std::unique_ptr<GSRasterBackend> backend=cpuOnly?std::unique_ptr<GSRasterBackend>(new GSCpuBackend):std::unique_ptr<GSRasterBackend>(new GSHwBackend);
 std::vector<uint8_t> memory;std::map<uint32_t,uint64_t> ops;std::map<std::tuple<unsigned,unsigned,unsigned>,uint64_t> draws;
 unsigned sequenceSaved=0;
 uint64_t records=0,submits=0,presents=0,lastTick=0;bool saved=false;GSPresentationRequest lastRequest{};
 auto save=[&](const GSPresentationRequest& req){
  const auto frame=backend->Present(req);if(!frame)throw std::runtime_error("No replay framebuffer");
  // Native presentation contract uses fixed FB_WIDTH (640) row pitch.
  std::vector<uint8_t> packed(frame.width*frame.height*4u);
  const auto pitch=frame.pixels.size()/frame.height;
  for(unsigned y=0;y<frame.height;++y)std::memcpy(packed.data()+y*frame.width*4u,frame.pixels.data()+y*pitch,frame.width*4u);
  fs::path output=destination;
  if(sequenceCount){fs::create_directories(destination);output=destination/("frame-"+std::to_string(req.vsyncTick)+".png");}
  if(sequenceCount){fs::path raw=output;raw.replace_extension(".rgba");std::ofstream f(raw,std::ios::binary);f.write(reinterpret_cast<const char*>(packed.data()),packed.size());}
  Image img{packed.data(),static_cast<int>(frame.width),static_cast<int>(frame.height),1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  if(!ExportImage(img,reinterpret_cast<const char*>(output.u8string().c_str())))throw std::runtime_error("Cannot export replay image");
  std::cout<<"FRAME tick="<<req.vsyncTick<<" source="<<frame.sourceFbp<<" size="<<frame.width<<"x"<<frame.height<<" records="<<records<<" draws="<<submits<<"\n";
  saved=true;++sequenceSaved;
 };
 while(file){
  uint32_t head[2];if(!file.read(reinterpret_cast<char*>(head),sizeof(head)))break;
  if(head[1]>16u*1024u*1024u)throw std::runtime_error("Capture record exceeds bounds");
  std::vector<uint8_t> bytes(head[1]);if(head[1]&&!file.read(reinterpret_cast<char*>(bytes.data()),head[1]))throw std::runtime_error("Truncated capture payload");
  ++records;++ops[head[0]];
  using namespace DisneyCapture;
  switch(head[0]){
   case Init:memory=bytes;backend->Initialize(memory.data(),static_cast<uint32_t>(memory.size()));break;
   case Reset:backend->Reset();break;
   case Submit:{auto b=capturedValue<GSPrimitiveBatch>(bytes);if(b.state.context.frame.fbp==210u){static unsigned shown=0;if(shown++<4)std::cout<<"DEPTHCOLOR bw="<<unsigned(b.state.context.frame.fbw)<<" psm="<<unsigned(b.state.context.frame.psm)<<" mask="<<std::hex<<b.state.context.frame.fbmsk<<" test="<<b.state.context.test<<std::dec<<" zbp="<<b.state.context.zbuf.zbp<<" zpsm="<<unsigned(b.state.context.zbuf.psm)<<" zmask="<<b.state.context.zbuf.zmask<<" xy="<<b.vertices[0].x<<","<<b.vertices[0].y<<"-"<<b.vertices[1].x<<","<<b.vertices[1].y<<" off="<<b.state.context.xyoffset.ofx<<","<<b.state.context.xyoffset.ofy<<" color="<<unsigned(b.vertices[1].r)<<","<<unsigned(b.vertices[1].g)<<","<<unsigned(b.vertices[1].b)<<","<<unsigned(b.vertices[1].a)<<"\n";}backend->Submit(b);++submits;++draws[{b.state.context.frame.fbp,b.state.context.frame.fbw,b.state.context.frame.psm}];break;}
   case Clut:{auto c=capturedValue<ClutData>(bytes);backend->LoadClut(c.tex,c.clut);break;}
   case Transfer:backend->BeginTransfer(capturedValue<GSTransferCommand>(bytes));break;
   case Upload:backend->UploadImage(bytes.data(),static_cast<uint32_t>(bytes.size()));break;
   case Flush:backend->Flush();break;
   case TexFlush:backend->TextureFlush();break;
   case Sync:backend->Sync(capturedValue<GSSyncReason>(bytes));break;
   case Clear:{auto c=capturedValue<ClearData>(bytes);backend->ClearFramebuffer(c.context,c.rgba);break;}
   case Write:{auto c=capturedValue<WriteData>(bytes);backend->WriteVram(c.psm,c.base,c.bw,c.x,c.y,c.value);break;}
   case Present:{lastRequest=capturedValue<GSPresentationRequest>(bytes);++presents;lastTick=lastRequest.vsyncTick;if(lastTick>=stopTick){save(lastRequest);if(!sequenceCount || sequenceSaved>=sequenceCount)goto done;}else backend->Present(lastRequest);break;}
   default:throw std::runtime_error("Unknown capture operation");
  }
 }
 done:
 if(!saved && presents)save(lastRequest);
 for(auto [op,n]:ops)std::cout<<"OP "<<op<<" "<<n<<"\n";
 for(auto [key,n]:draws){auto [base,bw,psm]=key;std::cout<<"DRAW fbp="<<base<<" fbw="<<bw<<" psm="<<psm<<" count="<<n<<"\n";}
 std::cout<<"COMPLETE records="<<records<<" draws="<<submits<<" presents="<<presents<<" lastTick="<<lastTick<<"\n";
 return saved?0:1;
}
