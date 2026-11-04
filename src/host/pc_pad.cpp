#include "../../tools/PS2Recomp/ps2xRuntime/src/lib/Kernel/Stubs/Pad.h"
#include "runtime/ps2_pad.h"
#include "ps2_runtime.h"
#include <chrono>
#include "ps2_host_backend.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include "hang_diagnostics.h"

namespace
{
    std::atomic<uint16_t> latchedKeyboard{0};
    constexpr uint8_t kPadAnalogMarker = 0x73;
    constexpr uint8_t kPadStickCenter = 0x80;

    constexpr uint16_t PAD_LEFT = 0x0080u;
    constexpr uint16_t PAD_DOWN = 0x0040u;
    constexpr uint16_t PAD_RIGHT = 0x0020u;
    constexpr uint16_t PAD_UP = 0x0010u;
    constexpr uint16_t PAD_START = 0x0008u;
    constexpr uint16_t PAD_R3 = 0x0004u;
    constexpr uint16_t PAD_L3 = 0x0002u;
    constexpr uint16_t PAD_SELECT = 0x0001u;
    constexpr uint16_t PAD_SQUARE = 0x8000u;
    constexpr uint16_t PAD_CROSS = 0x4000u;
    constexpr uint16_t PAD_CIRCLE = 0x2000u;
    constexpr uint16_t PAD_TRIANGLE = 0x1000u;
    constexpr uint16_t PAD_R1 = 0x0800u;
    constexpr uint16_t PAD_L1 = 0x0400u;
    constexpr uint16_t PAD_R2 = 0x0200u;
    constexpr uint16_t PAD_L2 = 0x0100u;
}

// GLFW can deliver press+release between game input polls. Preserve press events
// on the window thread until the original game's input manager samples them.
// A fixed 50 ms pulse is too short when an original frame takes 300-400 ms.
void disneyCaptureHostPadEvents()
{
    uint16_t pressed=0;
    // Deterministic diagnostic input, disabled in all normal launches. No OS UI injection.
    if(DisneyHang::enabled() && std::getenv("DISNEY_DIAG_MENU_INPUT")){
        static unsigned frame=0;++frame;
        if(frame==2100)pressed|=PAD_DOWN;
        if(frame==2220)pressed|=PAD_UP;
        if(frame==2340)pressed|=PAD_CROSS;
        if(frame==2820)pressed|=PAD_CROSS;
        if(frame==3300)pressed|=PAD_CROSS;
        if(frame>=5400 && frame<5580)pressed|=PAD_UP;
        if(frame>=5580 && frame<5700)pressed|=PAD_LEFT;
        if(frame==5760)pressed|=PAD_CROSS;
    }
    for(int key=GetKeyPressed();key!=0;key=GetKeyPressed()) {
        switch(key) {
        case KEY_UP:case KEY_W:pressed|=PAD_UP;break;
        case KEY_DOWN:case KEY_S:pressed|=PAD_DOWN;break;
        case KEY_LEFT:case KEY_A:pressed|=PAD_LEFT;break;
        case KEY_RIGHT:case KEY_D:pressed|=PAD_RIGHT;break;
        case KEY_X:case KEY_SPACE:pressed|=PAD_CROSS;break;
        case KEY_C:pressed|=PAD_CIRCLE;break;
        case KEY_Z:case KEY_KP_0:pressed|=PAD_SQUARE;break;
        case KEY_V:case KEY_KP_1:pressed|=PAD_TRIANGLE;break;
        case KEY_Q:pressed|=PAD_L1;break;case KEY_E:pressed|=PAD_R1;break;
        case KEY_LEFT_SHIFT:case KEY_ONE:pressed|=PAD_L2;break;
        case KEY_RIGHT_SHIFT:case KEY_THREE:pressed|=PAD_R2;break;
        case KEY_ENTER:pressed|=PAD_START;break;case KEY_TAB:pressed|=PAD_SELECT;break;
        case KEY_LEFT_CONTROL:pressed|=PAD_L3;break;case KEY_RIGHT_CONTROL:pressed|=PAD_R3;break;
        default:break;
        }
    }
    if(pressed)latchedKeyboard.fetch_or(pressed,std::memory_order_release);
    if(DisneyHang::enabled()){
        if(pressed){++DisneyHang::keyboardEdges;std::fprintf(stderr,"[hang-key-event] bits=%04x\n",pressed);}
        uint32_t physical=0;for(int i=0;i<4;++i)if(IsGamepadAvailable(i)){for(int b=1;b<18;++b)if(IsGamepadButtonDown(i,b))physical|=1u<<b;break;}
        if(physical!=DisneyHang::physical.exchange(physical)){++DisneyHang::physicalEdges;std::fprintf(stderr,"[hang-host-pad-event] buttons=%08x\n",physical);}
    }
}

bool PSPadBackend::readState(int port, int slot, uint8_t *data, size_t size)
{
    if (!data || size < 32)
        return false;

    std::memset(data, 0, 32);
    data[0] = 0x01;
    data[1] = kPadAnalogMarker;
    data[2] = 0xFF;
    data[3] = 0xFF;
    data[4] = data[5] = data[6] = data[7] = kPadStickCenter;

    uint16_t btns = 0xFFFFu;
    if(port==0 && slot==0)btns &= static_cast<uint16_t>(~latchedKeyboard.load(std::memory_order_acquire));
    int kGamepad = -1;
    for (int index = 0; index < 4; ++index) { if (IsGamepadAvailable(index)) { kGamepad = index; break; } }
    const bool useGamepad = kGamepad >= 0;
    auto clearBit = [&btns](uint16_t mask)
    { btns &= ~mask; };

    if (useGamepad)
    {
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_LEFT_FACE_UP))
            clearBit(PAD_UP);
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN))
            clearBit(PAD_DOWN);
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT))
            clearBit(PAD_LEFT);
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT))
            clearBit(PAD_RIGHT);
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
            clearBit(PAD_CROSS);
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
            clearBit(PAD_CIRCLE);
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT))
            clearBit(PAD_SQUARE);
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_RIGHT_FACE_UP))
            clearBit(PAD_TRIANGLE);
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1))
            clearBit(PAD_L1);
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1))
            clearBit(PAD_R1);
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_2))
            clearBit(PAD_L2);
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_2))
            clearBit(PAD_R2);
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT))
            clearBit(PAD_START);
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_MIDDLE_LEFT))
            clearBit(PAD_SELECT);
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_LEFT_THUMB))
            clearBit(PAD_L3);
        if (IsGamepadButtonDown(kGamepad, GAMEPAD_BUTTON_RIGHT_THUMB))
            clearBit(PAD_R3);

        float lx = GetGamepadAxisMovement(kGamepad, GAMEPAD_AXIS_LEFT_X);
        float ly = GetGamepadAxisMovement(kGamepad, GAMEPAD_AXIS_LEFT_Y);
        float rx = GetGamepadAxisMovement(kGamepad, GAMEPAD_AXIS_RIGHT_X);
        float ry = GetGamepadAxisMovement(kGamepad, GAMEPAD_AXIS_RIGHT_Y);
        data[6] = static_cast<uint8_t>(128 + (std::abs(lx) < 0.18f ? 0.0f : std::clamp(lx, -1.0f, 1.0f)) * 127);
        data[7] = static_cast<uint8_t>(128 + (std::abs(ly) < 0.18f ? 0.0f : std::clamp(ly, -1.0f, 1.0f)) * 127);
        data[4] = static_cast<uint8_t>(128 + (std::abs(rx) < 0.18f ? 0.0f : std::clamp(rx, -1.0f, 1.0f)) * 127);
        data[5] = static_cast<uint8_t>(128 + (std::abs(ry) < 0.18f ? 0.0f : std::clamp(ry, -1.0f, 1.0f)) * 127);
    }
    {
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
            clearBit(PAD_UP);
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
            clearBit(PAD_DOWN);
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
            clearBit(PAD_LEFT);
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
            clearBit(PAD_RIGHT);
        if (IsKeyDown(KEY_X) || IsKeyDown(KEY_SPACE))
            clearBit(PAD_CROSS);
        if (IsKeyDown(KEY_C))
            clearBit(PAD_CIRCLE);
        if (IsKeyDown(KEY_Z) || IsKeyDown(KEY_KP_0))
            clearBit(PAD_SQUARE);
        if (IsKeyDown(KEY_V) || IsKeyDown(KEY_KP_1))
            clearBit(PAD_TRIANGLE);
        if (IsKeyDown(KEY_Q))
            clearBit(PAD_L1);
        if (IsKeyDown(KEY_E))
            clearBit(PAD_R1);
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_ONE))
            clearBit(PAD_L2);
        if (IsKeyDown(KEY_RIGHT_SHIFT) || IsKeyDown(KEY_THREE))
            clearBit(PAD_R2);
        if (IsKeyDown(KEY_ENTER))
            clearBit(PAD_START);
        if (IsKeyDown(KEY_TAB))
            clearBit(PAD_SELECT);
    }

    if (IsKeyDown(KEY_LEFT_CONTROL)) clearBit(PAD_L3);
    if (IsKeyDown(KEY_RIGHT_CONTROL)) clearBit(PAD_R3);
    if (IsKeyDown(KEY_J) || IsKeyDown(KEY_L)) data[4] = IsKeyDown(KEY_J) == IsKeyDown(KEY_L) ? 128 : (IsKeyDown(KEY_J) ? 1 : 255);
    if (IsKeyDown(KEY_I) || IsKeyDown(KEY_K)) data[5] = IsKeyDown(KEY_I) == IsKeyDown(KEY_K) ? 128 : (IsKeyDown(KEY_I) ? 1 : 255);
    // The original game steers with the left analog stick (bytes 6/7), not the
    // D-pad. Mirror WASD/arrows there so the game is playable on keyboard.
    {
        int lx = 0, ly = 0;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) --lx;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) ++lx;
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) --ly;
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) ++ly;
        if (lx != 0) data[6] = static_cast<uint8_t>(128 + lx * 127);
        if (ly != 0) data[7] = static_cast<uint8_t>(128 + ly * 127);
    }
    data[2] = static_cast<uint8_t>(btns & 0xFF);
    data[3] = static_cast<uint8_t>(btns >> 8);
    return true;
}




// SLUS_206.07 FUN_00112770 consumes this platform device packet, then runs the
// original edge detection, pressure conversion, input handlers and gameplay.
// Offsets/order are from Ghidra's original routine, not guessed gameplay state.
void disneySampleOriginalPad(uint8_t *ram,R5900Context *ctx,PS2Runtime *runtime)
{
    if(DisneyHang::enabled())++DisneyHang::padReads;
    const uint32_t channel=getRegU32(ctx,4)&0x1ffffffu;
    if(channel>PS2_RAM_SIZE-4u)return;
    uint32_t device;std::memcpy(&device,ram+channel,4);device&=0x1ffffffu;
    if(device==0u || device>PS2_RAM_SIZE-0xe8u)return;
    uint32_t port;std::memcpy(&port,ram+device+0xe4u,4);
    if(port!=0u)return;
    if(DisneyHang::enabled()){
        static auto previous=std::chrono::steady_clock::now()-std::chrono::seconds(2);
        const auto now=std::chrono::steady_clock::now();
        if(now-previous>=std::chrono::seconds(1)){
            previous=now;
            auto word=[&](uint32_t at){uint32_t v;std::memcpy(&v,ram+at,4);return v;};
            const auto pads=ps2_stubs::getPadDebugSnapshot();const auto& pad=pads.ports[0][0];
            std::fprintf(stderr,"[hang-pad-state] ch=%08x dev=%08x type=%u connected=%u caps=%x pressure=%u setup=%u/%u valid=%u packet=%02x%02x%02x%02x channelButtons=%04x edges=%04x sdkOpen=%u analog=%u pressure=%u reads=%u sdkPacket=%02x%02x%02x%02x timer=%u/%u\n",channel,device,word(device+0xc),word(device+0x18),word(device+0xcc),word(device+0xd0),word(device+0xdc),word(device+0xe0),word(device+0xe8),ram[device+0x20],ram[device+0x21],ram[device+0x22],ram[device+0x23],word(channel+0x64),word(channel+0x68),pad.open,pad.analogMode,pad.pressureEnabled,pad.readCount,pad.lastData[0],pad.lastData[1],pad.lastData[2],pad.lastData[3],word(0x300c14),word(0x300c04));
        }
    }
    if(DisneyHang::enabled()){
        static uint32_t lastButtons=0,lastEdges=0;
        uint32_t current=0,edges=0;std::memcpy(&current,ram+channel+0x64,4);std::memcpy(&edges,ram+channel+0x68,4);
        if(current!=lastButtons || edges!=lastEdges){std::fprintf(stderr,"[hang-game-channel] ch=%08x current=%04x edges=%04x\n",channel,current,edges);lastButtons=current;lastEdges=edges;}
    }
    uint8_t packet[32]{};
    if(!runtime->padBackend().readState(0,0,packet,sizeof(packet)))return;
    latchedKeyboard.store(0,std::memory_order_release);
    const uint16_t buttons=static_cast<uint16_t>(packet[2]|(packet[3]<<8));
    if(DisneyHang::enabled()){DisneyHang::lastPad.store(buttons);DisneyHang::deviceAddr.store(device);}
    const uint32_t connected=1u;std::memcpy(ram+device+0x18u,&connected,4);
    ram[device+0x20u]=0u;ram[device+0x21u]=0x73u;
    ram[device+0x22u]=packet[2];ram[device+0x23u]=packet[3];
    std::memcpy(ram+device+0x24u,packet+4,4);
    const uint16_t pressureMasks[]={PAD_RIGHT,PAD_LEFT,PAD_UP,PAD_DOWN,PAD_TRIANGLE,PAD_CIRCLE,PAD_CROSS,PAD_SQUARE,PAD_L1,PAD_R1,PAD_L2,PAD_R2};
    for(unsigned i=0;i<12;++i)ram[device+0x28u+i]=(buttons&pressureMasks[i])?0u:255u;
    if(DisneyHang::enabled()){
        static uint16_t last=0xffff;
        if(buttons!=last){std::fprintf(stderr,"[hang-game-pad] pc=%08x device=%08x packet=%04x previous=%04x\n",ctx->pc,device,buttons,last);last=buttons;}
    }
    if(std::getenv("DISNEY_AOT_TRACE_PAD")) {
        static unsigned reports=0;
        if(reports<4u || (buttons!=0xffffu && reports<160u)) {
            std::fprintf(stderr,"[native-game-input] channel=%08x device=%08x port=%u buttons=%04x raw=%02x%02x\n",channel,device,port,buttons,ram[device+0x22u],ram[device+0x23u]);++reports;
        }
    }
}
