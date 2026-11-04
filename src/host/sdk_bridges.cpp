#include "ps2_runtime.h"
#include "hang_diagnostics.h"
#include "ps2_stubs.h"
#include "ps2_recompiled_functions.h"
// USA SLUS_206.07: SDK signature match plus observed SIF boot wait at 0x2ced18.
void FUN_002cebc0_0x2cebc0(uint8_t* rdram, R5900Context* ctx, PS2Runtime* runtime) {
    ps2_stubs::sceSifInitRpc(rdram, ctx, runtime);
    ctx->pc = getRegU32(ctx, 31);
}
#include <cstdio>
#include <algorithm>
// Host TTY output replaces the unavailable PS2 development-console transport.
void FUN_002e9818_0x2e9818(uint8_t* rdram, R5900Context* ctx, PS2Runtime* runtime) {
    const uint32_t address = getRegU32(ctx, 4) & 0x1ffffffu;
    const uint32_t requested = getRegU32(ctx, 5);
    const uint32_t length = std::min(requested, 0x2000000u - address);
    const auto written = std::fwrite(rdram + address, 1, length, stdout);
    std::fflush(stdout);
    setReturnS32(ctx, static_cast<int32_t>(written));
    ctx->pc = getRegU32(ctx, 31);
}
#include "ps2_syscalls.h"
void FUN_002cf300_0x2cf300(uint8_t* ram, R5900Context* ctx, PS2Runtime* rt) {
 ctx->pc = getRegU32(ctx, 31); ps2_stubs::sceSifBindRpc(ram, ctx, rt);
}
void FUN_002cf4d0_0x2cf4d0(uint8_t* ram, R5900Context* ctx, PS2Runtime* rt) {
 ctx->pc = getRegU32(ctx, 31); if(DisneyHang::enabled())++DisneyHang::rpc; ps2_syscalls::SifCallRpc(ram, ctx, rt); if(DisneyHang::enabled())++DisneyHang::rpcDone;
}
void FUN_002cf6c0_0x2cf6c0(uint8_t* ram, R5900Context* ctx, PS2Runtime* rt) {
 ctx->pc = getRegU32(ctx, 31); ps2_stubs::sceSifCheckStatRpc(ram, ctx, rt);
}
#include "ps2x/iop/iop_types.h"
#include <string>
namespace {
void loadGuestModule(uint8_t* ram, R5900Context* ctx, PS2Runtime* rt, bool fromBuffer) {
 const uint32_t source = getRegU32(ctx,4) & 0x1ffffffu;
 const uint32_t length = getRegU32(ctx,5);
 const uint32_t args = getRegU32(ctx,6) & 0x1ffffffu;
 const uint32_t resultAddress = getRegU32(ctx,7);
 ctx->pc=getRegU32(ctx,31);
 if (length > 65536u || length > 0x2000000u-args) { setReturnS32(ctx,-1); return; }
 std::string path;
 if (!fromBuffer) {
  for(uint32_t i=source; i<0x2000000u && path.size()<1024 && ram[i]; ++i) path.push_back(static_cast<char>(ram[i]));
 }
 const auto loaded = fromBuffer ? rt->loadIopModuleBuffer(source,length ? ram+args : nullptr,length) : rt->loadIopModule(path,length ? ram+args : nullptr,length);
 if (resultAddress && resultAddress < 0x2000000u-4) std::memcpy(ram+resultAddress,&loaded.startResult,4);
 std::fprintf(stderr,"[module] %s id=%d start=%d handled=%d\n",fromBuffer ? "<buffer>" : path.c_str(),loaded.moduleId,loaded.startResult,loaded.handled);
 setReturnS32(ctx,loaded.handled ? loaded.moduleId : -1);
}
}
void FUN_002d7710_0x2d7710(uint8_t* ram,R5900Context* ctx,PS2Runtime* rt) { loadGuestModule(ram,ctx,rt,false); }
void FUN_002d7100_0x2d7100(uint8_t* ram,R5900Context* ctx,PS2Runtime* rt) { loadGuestModule(ram,ctx,rt,true); }
// Retail disc is single-layer; preserve the layer-aware SDK ABI.
void FUN_002cdc88_0x2cdc88(uint8_t* ram,R5900Context* ctx,PS2Runtime* rt) {
 ctx->pc=getRegU32(ctx,31);
 if(getRegU32(ctx,6)!=0) { setReturnS32(ctx,0); return; }
 ps2_stubs::sceCdSearchFile(ram,ctx,rt);
}
