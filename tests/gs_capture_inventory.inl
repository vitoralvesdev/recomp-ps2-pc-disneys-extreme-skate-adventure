int inventoryGsCapture(const fs::path& source){
 std::ifstream file(source,std::ios::binary);DisneyCapture::Header h{};file.read(reinterpret_cast<char*>(&h),sizeof(h));
 std::map<std::string,unsigned long long> counts;uint64_t lastTick=0,records=0;
 while(file){uint32_t head[2];if(!file.read(reinterpret_cast<char*>(head),8))break;if(head[1]>16u*1024u*1024u)throw std::runtime_error("Bad GS record");
  ++records;
  if(head[0]==DisneyCapture::Transfer){GSTransferCommand c{};if(head[1]!=sizeof(c))throw std::runtime_error("Transfer ABI");file.read(reinterpret_cast<char*>(&c),sizeof(c));
   if(c.bitbltbuf.dbp<11200 || c.bitbltbuf.sbp<11200){char key[512];std::snprintf(key,sizeof(key),"dir=%u src=%u/%u/%u xy=%u,%u dst=%u/%u/%u xy=%u,%u size=%u,%u",c.direction,c.bitbltbuf.sbp,c.bitbltbuf.sbw,c.bitbltbuf.spsm,c.trxpos.ssax,c.trxpos.ssay,c.bitbltbuf.dbp,c.bitbltbuf.dbw,c.bitbltbuf.dpsm,c.trxpos.dsax,c.trxpos.dsay,c.trxreg.rrw,c.trxreg.rrh);auto& n=counts[key];if(!n)std::cout<<"FIRST tick="<<lastTick<<" "<<key<<"\n";++n;}
  }else if(head[0]==DisneyCapture::Present){GSPresentationRequest r{};if(head[1]!=sizeof(r))throw std::runtime_error("Present ABI");file.read(reinterpret_cast<char*>(&r),sizeof(r));lastTick=r.vsyncTick;}
  else file.seekg(head[1],std::ios::cur);
 }
 for(auto& [key,count]:counts)if(count>1)std::cout<<"COUNT="<<count<<" "<<key<<"\n";
 std::cout<<"RECORDS="<<records<<" LASTTICK="<<lastTick<<"\n";return 0;
}
