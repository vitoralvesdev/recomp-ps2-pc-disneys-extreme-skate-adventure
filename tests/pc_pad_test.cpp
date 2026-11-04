#include "runtime/ps2_pad.h"
#include "raylib.h"
#include <array>
#include <cstdio>
#include <cstdlib>
static std::array<bool,512> keys{};
static int device = -1;
static bool cross = false;
static float axis = 0;
extern "C" bool IsKeyDown(int key) { return key >= 0 && key < 512 && keys[key]; }
extern "C" bool IsGamepadAvailable(int index) { return index == device; }
extern "C" bool IsGamepadButtonDown(int index,int button) { return index == device && cross && button == GAMEPAD_BUTTON_RIGHT_FACE_DOWN; }
extern "C" float GetGamepadAxisMovement(int,int axisIndex) { return axisIndex == GAMEPAD_AXIS_LEFT_X ? axis : 0; }
static void check(bool condition, const char* message) { if (!condition) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); } }
int main() {
 PSPadBackend pad; uint8_t data[32];
 check(!pad.readState(0,0,nullptr,32),"null buffer");
 check(!pad.readState(0,0,data,8),"short buffer");
 check(pad.readState(0,0,data,32),"neutral input");
 check(data[2] == 255 && data[3] == 255 && data[6] == 128,"neutral packet");
 keys[KEY_SPACE] = true; pad.readState(0,0,data,32);
 check((data[3] & 0x40) == 0,"keyboard cross");
 device = 2; keys[KEY_SPACE] = false; keys[KEY_Z] = true; cross = true; axis = .1f;
 pad.readState(0,0,data,32);
 check((data[3] & 0xC0) == 0,"keyboard and third-slot controller together");
 check(data[6] == 128,"stick deadzone");
 axis = 1; pad.readState(0,0,data,32); check(data[6] == 255,"full stick travel");
 device = -1; cross = false; keys.fill(false); keys[KEY_ENTER] = true;
 pad.readState(0,0,data,32); check((data[2] & 8) == 0,"keyboard after unplug");
 std::puts("PASS: neutral, keyboard, controller slot discovery, simultaneous input, deadzone, range, unplug");
}
