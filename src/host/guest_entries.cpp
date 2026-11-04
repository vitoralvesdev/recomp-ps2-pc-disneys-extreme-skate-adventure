#include "ps2_runtime.h"
#include "ps2_recompiled_functions.h"
#include "hang_diagnostics.h"
void disneySampleOriginalPad(uint8_t*,R5900Context*,PS2Runtime*);
namespace {
void disneyOriginalInputEntry(uint8_t* ram,R5900Context* context,PS2Runtime* runtime) {
    if(context->pc==0x112770u)disneySampleOriginalPad(ram,context,runtime);
    FUN_00112770_0x112770(ram,context,runtime);
    if(DisneyHang::enabled())++DisneyHang::padDone;

}
}
void registerDisneyNativeSaves(PS2Runtime&);
void registerDisneyNativeAudio(PS2Runtime&);
void registerDisneyGuestEntries(PS2Runtime& runtime) {
 registerDisneyNativeSaves(runtime);
 registerDisneyNativeAudio(runtime);
 runtime.registerFunction(0x112770u,&disneyOriginalInputEntry);
 if (!runtime.hasFunction(0x1337c0u)) runtime.registerFunction(0x1337c0u, &entry_00133770_0x133770);
 if (!runtime.hasFunction(0x133844u)) runtime.registerFunction(0x133844u, &entry_00133820_0x133820);
 if (!runtime.hasFunction(0x1338ecu)) runtime.registerFunction(0x1338ecu, &entry_001338c8_0x1338c8);
 if (!runtime.hasFunction(0x133980u)) runtime.registerFunction(0x133980u, &entry_00133960_0x133960);
 if (!runtime.hasFunction(0x133a50u)) runtime.registerFunction(0x133a50u, &entry_00133a30_0x133a30);
 if (!runtime.hasFunction(0x133b04u)) runtime.registerFunction(0x133b04u, &entry_00133ae4_0x133ae4);
 if (!runtime.hasFunction(0x133bb8u)) runtime.registerFunction(0x133bb8u, &entry_00133b98_0x133b98);
 if (!runtime.hasFunction(0x133c68u)) runtime.registerFunction(0x133c68u, &entry_00133c48_0x133c48);
 if (!runtime.hasFunction(0x133d40u)) runtime.registerFunction(0x133d40u, &entry_00133d20_0x133d20);
 if (!runtime.hasFunction(0x133e04u)) runtime.registerFunction(0x133e04u, &entry_00133e04_0x133e04);
}

#include "ps2_stubs.h"
// SLUS_206.07 libmc functions identified by Ghidra, native-mc-*.txt.
// Replace SDK transport only. Game serialization/checksums/save state remain
// original code; no save-success flags or fabricated progress are written.
namespace {
template<void (*Handler)(uint8_t*,R5900Context*,PS2Runtime*)>
void nativeMcSdk(uint8_t* ram,R5900Context* context,PS2Runtime* runtime) {
    context->pc=getRegU32(context,31);
    Handler(ram,context,runtime);
}
}
void registerDisneyNativeSaves(PS2Runtime& runtime) {
    runtime.registerFunction(0x2be770u,&nativeMcSdk<ps2_stubs::sceMcInit>);
    runtime.registerFunction(0x2be928u,&nativeMcSdk<ps2_stubs::sceMcEnd>);
    runtime.registerFunction(0x2be998u,&nativeMcSdk<ps2_stubs::sceMcChangeThreadPriority>);
    runtime.registerFunction(0x2bea50u,&nativeMcSdk<ps2_stubs::sceMcGetSlotMax>);
    runtime.registerFunction(0x2beb18u,&nativeMcSdk<ps2_stubs::sceMcOpen>);
    runtime.registerFunction(0x2bec78u,&nativeMcSdk<ps2_stubs::sceMcClose>);
    runtime.registerFunction(0x2bed30u,&nativeMcSdk<ps2_stubs::sceMcSeek>);
    runtime.registerFunction(0x2bee98u,&nativeMcSdk<ps2_stubs::sceMcRead>);
    runtime.registerFunction(0x2befb0u,&nativeMcSdk<ps2_stubs::sceMcWrite>);
    runtime.registerFunction(0x2bf198u,&nativeMcSdk<ps2_stubs::sceMcSync>);
    runtime.registerFunction(0x2bf2d0u,&nativeMcSdk<ps2_stubs::sceMcGetInfo>);
    runtime.registerFunction(0x2bf4c0u,&nativeMcSdk<ps2_stubs::sceMcGetDir>);
    runtime.registerFunction(0x2bf690u,&nativeMcSdk<ps2_stubs::sceMcChdir>);
    runtime.registerFunction(0x2bf7d0u,&nativeMcSdk<ps2_stubs::sceMcFormat>);
    std::fprintf(stderr,"[native-save] Original libmc SDK connected to Windows filesystem\n");
}
