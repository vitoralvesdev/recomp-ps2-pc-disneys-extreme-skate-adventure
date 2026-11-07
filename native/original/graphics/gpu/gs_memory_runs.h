#pragma once
#include "runtime/gs/ps2_gs_memory.h"
namespace GSMem {
const u16* PageTableData(PixelStorageMode psm);
void WriteRun(u8*,u32,u32,u32,u32,u32,const u8*,u32,u32);
void ReadIndexRun(const u8*,u32,u32,u32,u32,u32,u32,u8*);
bool ReadColorRun(const u8*,u32,u32,u32,u32,u32,u32,u32*);
}
