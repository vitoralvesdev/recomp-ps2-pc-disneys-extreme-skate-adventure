#include "ps2_runtime.h"
#include "ps2_recompiled_functions.h"
#include "hang_diagnostics.h"
void disneySampleOriginalPad(uint8_t*,R5900Context*,PS2Runtime*);
void entry_303910_0x303948(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_00303948_0x303948(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_00303968_0x303968(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_00303a70_0x303a70(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_00303bb0_0x303bb0(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_00303d50_0x303d50(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_00303d70_0x303d70(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_00303d98_0x303d98(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_00303f90_0x303f90(uint8_t*,R5900Context*,PS2Runtime*);
void entry_3040b8_0x3040f0(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_003040f0_0x3040f0(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_00304180_0x304180(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_003041c0_0x3041c0(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_00304210_0x304210(uint8_t*,R5900Context*,PS2Runtime*);
void FUN_00304260_0x304260(uint8_t*,R5900Context*,PS2Runtime*);
namespace {
void disneyOriginalInputEntry(uint8_t* ram,R5900Context* context,PS2Runtime* runtime) {
    if(context->pc==0x112770u)disneySampleOriginalPad(ram,context,runtime);
    FUN_00112770_0x112770(ram,context,runtime);
    if(DisneyHang::enabled())++DisneyHang::padDone;

}
// PS2Recomp needs 0x303fc0 as an entry point to branch past the copied prologue.
void disneyRelocatedExceptionReturn(uint8_t* ram,R5900Context* context,PS2Runtime* runtime) {
    context->pc=0x303fc0u;
    FUN_00303f90_0x303f90(ram,context,runtime);
}
}
void registerDisneyNativeSaves(PS2Runtime&);
void registerDisneyNativeAudio(PS2Runtime&);
void registerDisneyGuestEntries(PS2Runtime& runtime) {
    const auto registerRelocated = [&runtime](uint32_t sourceBase, uint32_t destinationBase,
                                               uint32_t sourceBegin, uint32_t sourceEnd,
                                               PS2Runtime::RecompiledFunction function) {
        const uint32_t address = sourceBegin - sourceBase + destinationBase;
        const uint32_t size = sourceEnd - sourceBegin;
        runtime.registerFunction(address, function);
        runtime.registerFunction(address | 0x80000000u, function);
        runtime.registerFunctionRange(sourceBegin, sourceEnd, function);
        runtime.registerFunctionRange(address, address + size, function);
        runtime.registerFunctionRange(address | 0x80000000u, (address | 0x80000000u) + size, function);
    };
    // InitAlarm copies this kernel block from 0x303910 to 0x80076000.
    registerRelocated(0x303910u, 0x76000u, 0x303910u, 0x303948u, &entry_303910_0x303948);
    registerRelocated(0x303910u, 0x76000u, 0x303948u, 0x303968u, &FUN_00303948_0x303948);
    registerRelocated(0x303910u, 0x76000u, 0x303968u, 0x303a70u, &FUN_00303968_0x303968);
    registerRelocated(0x303910u, 0x76000u, 0x303a70u, 0x303bb0u, &FUN_00303a70_0x303a70);
    registerRelocated(0x303910u, 0x76000u, 0x303bb0u, 0x303d50u, &FUN_00303bb0_0x303bb0);
    registerRelocated(0x303910u, 0x76000u, 0x303d50u, 0x303d70u, &FUN_00303d50_0x303d50);
    registerRelocated(0x303910u, 0x76000u, 0x303d70u, 0x303d98u, &FUN_00303d70_0x303d70);
    registerRelocated(0x303910u, 0x76000u, 0x303d98u, 0x303f90u, &FUN_00303d98_0x303d98);
    registerRelocated(0x303910u, 0x76000u, 0x303f90u, 0x303fccu, &FUN_00303f90_0x303f90);
    const uint32_t exceptionReturnEntry = 0x303fc0u - 0x303910u + 0x76000u;
    runtime.registerFunction(exceptionReturnEntry, &disneyRelocatedExceptionReturn);
    runtime.registerFunction(exceptionReturnEntry | 0x80000000u, &disneyRelocatedExceptionReturn);
    // The original kernel copies its TLB block from 0x3040b8 to 0x80075000.
    registerRelocated(0x3040b8u, 0x75000u, 0x3040b8u, 0x3040f0u, &entry_3040b8_0x3040f0);
    registerRelocated(0x3040b8u, 0x75000u, 0x3040f0u, 0x304180u, &FUN_003040f0_0x3040f0);
    registerRelocated(0x3040b8u, 0x75000u, 0x304180u, 0x3041c0u, &FUN_00304180_0x304180);
    registerRelocated(0x3040b8u, 0x75000u, 0x3041c0u, 0x304210u, &FUN_003041c0_0x3041c0);
    registerRelocated(0x3040b8u, 0x75000u, 0x304210u, 0x304260u, &FUN_00304210_0x304210);
    registerRelocated(0x3040b8u, 0x75000u, 0x304260u, 0x304390u, &FUN_00304260_0x304260);
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
