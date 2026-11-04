// Focused reproduction of the captured final VU input. Does not boot a game.
int reproduceLastVuSignal(const fs::path& path){
 std::ifstream f(path,std::ios::binary);std::array<uint32_t,4> h{};VU1State state{};
 std::array<uint8_t,16384> code{},data{};
 f.read((char*)h.data(),16);if(h[3]!=sizeof(state))throw std::runtime_error("VU ABI mismatch");
 f.read((char*)&state,sizeof(state));f.read((char*)code.data(),code.size());f.read((char*)data.data(),data.size());
 if(!f)throw std::runtime_error("Short VU input");
 auto memory=std::make_unique<PS2Memory>();if(!memory->initialize())throw std::runtime_error("RAM unavailable");
 auto gs=std::make_unique<GS>();gs->init(memory->getGSVRAM(),4u*1024u*1024u,&memory->gs());gs->setDebugHistoryPaused(true);
 unsigned packets=0,signalWrites=0;
 memory->setGifPacketCallback([&](const uint8_t* bytes,uint32_t size){
  ++packets;std::cout<<"PACKET "<<packets<<" bytes="<<size<<" words=";
  for(unsigned i=0;i<std::min<unsigned>(size,96);i+=4){uint32_t word;std::memcpy(&word,bytes+i,4);std::cout<<std::hex<<word<<',';}
  std::cout<<std::dec<<"\n";
  for(unsigned i=16;i+16<=size;i+=16){uint64_t reg;std::memcpy(&reg,bytes+i+8,8);if((reg&255)==0x60)++signalWrites;}
  gs->processGIFPacket(bytes,size);
 });
 std::memcpy(memory->getVU1Code(),code.data(),code.size());memory->markVU1CodeModified();std::memcpy(memory->getVU1Data(),data.data(),data.size());
 VU1Interpreter vu;vu.state()=state;
 std::cout<<"CAPTURE entry="<<std::hex<<h[0]<<" top="<<h[1]<<" itop="<<h[2]<<std::dec<<"\n";
 vu.execute(memory->getVU1Code(),16384,memory->getVU1Data(),16384,*gs,memory.get(),h[0],h[1],h[2],1000000);
 std::cout<<"RESULT packets="<<packets<<" candidateSignalWrites="<<signalWrites<<" csr="<<std::hex<<memory->gs().csr.load()<<" vuPc="<<vu.state().pc<<std::dec<<"\n";
 return (memory->gs().csr.load()&1)?0:1;
}

