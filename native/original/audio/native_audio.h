#pragma once
#include <cstdint>
#include <cstddef>
class PS2Runtime;
struct R5900Context;
void registerDisneyNativeAudio(PS2Runtime&);
void disneyAudioInitialize();
void disneyAudioShutdown();
bool disneyNativeAudioInvoke(uint32_t address,uint8_t* ram,R5900Context* context);
void disneyAudioRender(float* stereo,size_t frames);
void disneyAudioResetForTest();
uint64_t disneyAudioRenderedNonzeroFrames();
