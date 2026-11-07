#pragma once
#include <cstdint>
namespace disney::original {
// Native stream framing only. Does not execute or decode guest CPU/VU code.
// Assembled packets are handed to the sink as spans valid only for the duration of the call: either into the
// caller's buffer when no partial tag is carried, or into the per-path carry buffer otherwise.
using GifPacketSink = void (*)(void *ctx, const uint8_t *packet, uint32_t size);
void assembleGifPackets(const void *owner, uint8_t path, const uint8_t *bytes, uint32_t size, GifPacketSink sink, void *ctx);
void resetGifStreams(const void *owner);
void diagnoseGifStreams(const void *owner);
}
