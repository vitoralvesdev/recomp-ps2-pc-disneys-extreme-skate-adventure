// Oracle: original SLUS_206.07 table at 0x2FCAE8 consumed by FUN_00112DA0.
int testOriginalPadTable(const fs::path& ramPath){
 auto original=read(ramPath,32u*1024u*1024u);std::vector<uint8_t> ram(original.size());auto c=std::make_unique<R5900Context>();
 ps2_stubs::scePadInit(ram.data(),c.get(),nullptr);SET_GPR_U32(c.get(),4,0);SET_GPR_U32(c.get(),5,0);SET_GPR_U32(c.get(),6,0x1000);ps2_stubs::scePadPortOpen(ram.data(),c.get(),nullptr);
 SET_GPR_U32(c.get(),6,1);ps2_stubs::scePadSetMainMode(ram.data(),c.get(),nullptr);ps2_stubs::scePadEnterPressMode(ram.data(),c.get(),nullptr);
 const unsigned expectedSlots[]={5,6,7,8,9,10,11,12,13,15,14,16};
 const unsigned packetSlots[]={8,9,10,11,12,13,14,15,16,18,17,19};
 const unsigned masks[]={0x20,0x80,0x10,0x40,0x1000,0x2000,0x4000,0x8000,0x400,0x100,0x800,0x200};
 const char* names[]={"right","left","up","down","triangle","circle","cross","square","L1","L2","R1","R2"};
 unsigned failures=0;
 for(unsigned i=0;i<12;++i){
  ps2_stubs::setPadOverrideState(static_cast<uint16_t>(0xffffu^masks[i]),128,128,128,128);SET_GPR_U32(c.get(),6,0x2000);ps2_stubs::scePadRead(ram.data(),c.get(),nullptr);
  const unsigned active=((ram[0x2002]<<8)|ram[0x2003])^0xffffu;
  unsigned observed=0;for(unsigned a=0x2fcae8;u32(original,a)!=0;a+=8)if(active&u32(original,a))observed=u32(original,a+4)+1;
  const unsigned expected=expectedSlots[i];
  std::cout<<"ORIGINAL_PAD "<<names[i]<<" mask="<<std::hex<<active<<std::dec<<" pressureSlot="<<observed<<" expected="<<expected<<"\n";
  failures+=observed!=expected || ram[0x2000+packetSlots[i]]!=255;
 }
 ps2_stubs::clearPadOverrideState();std::cout<<(failures?"FAIL":"PASS")<<": original game conversion table, 12 pressure-capable buttons, failures="<<failures<<"\n";return failures?1:0;
}
