#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "native_input.h"
#include "native_collision.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Native integration experiment, not the recovered original game loop.
// Geometry, materials and collision are converted offline. This executable
// never loads an original executable, archive, BIOS or console runtime.
struct Skater {
    Vector3 position{};
    Vector3 groundNormal{0,1,0};
    float heading = 0, speed = 0, verticalSpeed = 0, crouchTime = 0;
    bool grounded = true;
};
struct Physics {
    // Values recovered from PHYSICS.qb. The integration and wall response below
    // are an initial reconstruction, not a claim of original-game equivalence.
    float acceleration = 650, crouchAcceleration = 1100;
    float maxPush = 425.5f, maxCrouchPush = 575;
    float jumpMin = 350, jumpMax = 425, gravity = -1350, brake = 900;
    float turn = 1.8f, sharpTurn = 3.6f;
};

static Vector3 forward(float heading) { return {std::sin(heading),0,std::cos(heading)}; }
static float horizontalDistance(Vector3 a, Vector3 b) {
    return std::hypot(a.x-b.x,a.z-b.z);
}
static Vector3 findSpawn(const NativeWorld& world) {
    std::ifstream authored("data/spawn.txt");
    Vector3 authoredPoint{};
    float heading=0;
    if (authored >> authoredPoint.x >> authoredPoint.y >> authoredPoint.z >> heading) {
        auto ground=world.cast(Vector3Add(authoredPoint,{0,150,0}),
                               Vector3Add(authoredPoint,{0,-500,0}),0.6f);
        if (ground.hit) return Vector3Add(ground.point,{0,1,0});
    }
    // A stable experimental spawn is selected from actual collision surfaces,
    // near the central playable town, with headroom. Original restart nodes
    // and authored heading still need to be connected to the script system.
    Vector3 best{};
    float score = 1e30f;
    for (const auto& t : world.triangles) {
        if ((t.flags & 0x10) || std::fabs(t.normal.y) < 0.97f) continue;
        Vector3 p = Vector3Scale(Vector3Add(Vector3Add(t.a,t.b),t.c),1.0f/3.0f);
        if (p.y < -300 || p.y > 400) continue;
        auto ground = world.cast({p.x,p.y+20,p.z},{p.x,p.y-20,p.z},0.6f);
        if (!ground.hit) continue;
        auto ceiling = world.cast({p.x,p.y+5,p.z},{p.x,p.y+130,p.z});
        if (ceiling.hit) continue;
        const float value = horizontalDistance(p,{3000,0,8000})+std::fabs(p.y)*4;
        if (value < score) { score=value; best=ground.point; }
    }
    if (score == 1e30f) throw std::runtime_error("No safe native collision spawn found");
    best.y += 1;
    return best;
}

static void stepSkater(Skater& s, const NativeInputState& input,
                       const NativeWorld& world, const Physics& p, float dt) {
    s.heading -= input.steer*(s.speed < 150 ? p.sharpTurn : p.turn)*dt;
    if (s.grounded) {
        s.crouchTime = input.crouch ? std::min(0.2f,s.crouchTime+dt) : s.crouchTime;
        const float limit = input.crouch ? p.maxCrouchPush : p.maxPush;
        if (input.push > 0 && s.speed < limit)
            s.speed = std::min(limit,s.speed+input.push*(input.crouch?p.crouchAcceleration:p.acceleration)*dt);
        s.speed = std::max(0.0f,s.speed-input.brake*p.brake*dt);
        // Approximate rolling resistance for the first collision integration.
        s.speed = std::max(0.0f,s.speed-2.0f*dt);
        if (input.jumpReleased) {
            s.verticalSpeed=p.jumpMin+(p.jumpMax-p.jumpMin)*std::clamp(s.crouchTime/0.2f,0.0f,1.0f);
            s.grounded=false; s.crouchTime=0;
        } else if (!input.crouch) s.crouchTime=0;
    }
    Vector3 delta=Vector3Scale(forward(s.heading),s.speed*dt);
    // Sweep at wheel/body/head heights; the minimum separation is 8 inches,
    // matching Skater_Min_Distance_To_Wall in PHYSICS.qb.
    if (Vector3LengthSqr(delta)>0.000001f) {
        Vector3 probe=Vector3Add(delta,Vector3Scale(Vector3Normalize(delta),8));
        for (float height : {8.1f,32.0f,65.0f}) {
            Vector3 origin=Vector3Add(s.position,{0,height,0});
            auto wall=world.cast(origin,Vector3Add(origin,probe));
            if (wall.hit && std::fabs(wall.normal.y)<0.7f) {
                Vector3 n={wall.normal.x,0,wall.normal.z}; n=Vector3Normalize(n);
                float into=Vector3DotProduct(delta,n);
                if (into<0) delta=Vector3Subtract(delta,Vector3Scale(n,into));
                s.speed*=0.92f;
            }
        }
    }
    Vector3 next=Vector3Add(s.position,delta);
    if (!s.grounded) {
        s.verticalSpeed += p.gravity*dt;
        next.y += s.verticalSpeed*dt;
    }
    const float top = std::max(s.position.y,next.y)+(s.grounded?13.0f:1.0f);
    const float bottom = std::min(s.position.y,next.y)-(s.grounded?8.2f:1.0f);
    auto floor=world.cast({next.x,top,next.z},{next.x,bottom,next.z},0.45f);
    if (floor.hit && (s.grounded || s.verticalSpeed<=0)) {
        next.y=floor.point.y+0.25f; s.grounded=true; s.verticalSpeed=0;
        s.groundNormal=floor.normal;
    } else if (s.grounded) { s.grounded=false; s.verticalSpeed=0; }
    s.position=next;
}

struct Options {
    bool smoke=false;
    int frames=360;
};
static Options options(int argc, char** argv) {
    Options out;
    for (int i=1;i<argc;++i) {
        if (std::string(argv[i])=="--smoke") out.smoke=true;
        else if (std::string(argv[i])=="--frames" && i+1<argc) out.frames=std::clamp(std::stoi(argv[++i]),60,1800);
    }
    return out;
}

static void writeSmoke(bool passed, const Skater& s, float distance, int airFrames,
                       int landings, int frameCount, int triangles, float elapsed,
                       const char* detail) {
    std::ofstream out("native-smoke.json");
    out << "{\n  \"schema\": \"disney-native-integration-v1\",\n"
        << "  \"passed\": " << (passed?"true":"false") << ",\n"
        << "  \"frames\": " << frameCount << ",\n"
        << "  \"collisionTriangles\": " << triangles << ",\n"
        << "  \"travelledInches\": " << distance << ",\n"
        << "  \"airFrames\": " << airFrames << ",\n"
        << "  \"landings\": " << landings << ",\n"
        << "  \"simulationSeconds\": " << elapsed << ",\n"
        << "  \"finalPosition\": [" << s.position.x << ',' << s.position.y << ',' << s.position.z << "],\n"
        << "  \"scope\": \"converted assets, rendering, synthetic movement, ollie and landing; not full-game validation\",\n"
        << "  \"detail\": \"" << detail << "\"\n}\n";
}

int main(int argc,char** argv) {
    const auto opt=options(argc,argv);
    // Relative ASCII paths keep all runtime data in the distributable folder.
    ChangeDirectory(GetApplicationDirectory());
    SetTraceLogLevel(LOG_WARNING);
    NativeWorld world;
    if (!world.load("data/HUB.dscollision")) {
        std::cerr << "Native collision load failed: " << world.error << '\n'; return 2;
    }
    Vector3 spawn;
    try { spawn=findSpawn(world); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 3; }
    std::cout << "NATIVE: collision triangles=" << world.triangles.size()
              << " spawn=" << spawn.x << ',' << spawn.y << ',' << spawn.z << '\n';
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280,720,"Disney Skate - teste nativo experimental | HUB");
    if (!IsWindowReady()) return 4;
    rlSetClipPlanes(1.0,60000.0);
    SetTargetFPS(opt.smoke?120:60);
    Model level=LoadModel("data/HUB.glb");
    Model character=LoadModel("data/timon.glb");
    if (level.meshCount<=0 || character.meshCount<=0) {
        std::cerr << "Converted glTF meshes did not load\n";
        UnloadModel(character); UnloadModel(level); CloseWindow(); return 5;
    }
    std::cout << "NATIVE: level meshes=" << level.meshCount << " character meshes=" << character.meshCount << '\n';
    Skater skater; skater.position=spawn;
    {std::ifstream authored("data/spawn.txt");Vector3 p{};float h=0;
     if(authored>>p.x>>p.y>>p.z>>h)skater.heading=h;}
    const float spawnHeading=skater.heading;
    Physics physics;
    Camera3D camera{};camera.up={0,1,0};camera.fovy=60;camera.projection=CAMERA_PERSPECTIVE;
    auto placeCamera=[&](bool snap) {
        Vector3 desired=Vector3Add(skater.position,Vector3Add(Vector3Scale(forward(skater.heading),-230),{0,125,0}));
        camera.position=snap?desired:Vector3Lerp(camera.position,desired,0.12f);
        camera.target=Vector3Add(skater.position,{0,45,0});
        auto wall=world.cast(camera.target,desired);
        if (wall.hit) camera.position=Vector3Lerp(camera.target,wall.point,0.85f);
    };
    placeCamera(true);
    float accumulator=0,travelled=0, elapsed=0;
    int frames=0,airFrames=0,landings=0;
    bool showCollision=false, invalid=false, sawGamepad=false;
    while (!WindowShouldClose()) {
        float frameDt=opt.smoke?1.0f/60:std::min(GetFrameTime(),0.1f);
        elapsed+=frameDt;
        NativeInputState input=sampleNativeInput();
        if(input.gamepad && !sawGamepad)
            std::cout << "NATIVE: controller slot=" << input.gamepadIndex << " name="
                      << GetGamepadName(input.gamepadIndex) << '\n';
        sawGamepad=sawGamepad||input.gamepad;
        if (opt.smoke) {
            // Test actions enter the native simulation directly. No system key
            // presses, gamepad injection or fabricated test completion signals.
            input={};input.push=1;
            input.crouch=frames>=60 && frames<75;
            input.jumpReleased=frames==75;
            input.steer=frames>180?0.25f:0;
        }
        if (IsKeyPressed(KEY_F3)) showCollision=!showCollision;
        if (input.resetPressed) {skater=Skater{};skater.position=spawn;skater.heading=spawnHeading;placeCamera(true);}
        accumulator+=frameDt;
        bool jump=input.jumpReleased;
        while (accumulator>=1.0f/120) {
            input.jumpReleased=jump; jump=false;
            Vector3 before=skater.position; bool wasGrounded=skater.grounded;
            stepSkater(skater,input,world,physics,1.0f/120);
            travelled+=horizontalDistance(before,skater.position);
            if (!skater.grounded) ++airFrames;
            if (!wasGrounded && skater.grounded) ++landings;
            accumulator-=1.0f/120;
        }
        if (!std::isfinite(skater.position.y) || skater.position.y<-5000) {
            if(opt.smoke){invalid=true;break;}
            skater=Skater{};skater.position=spawn;skater.heading=spawnHeading;placeCamera(true);
        }
        placeCamera(false);
        BeginDrawing();ClearBackground({122,168,209,255});
        BeginMode3D(camera);
        rlDisableBackfaceCulling();
        DrawModel(level,{0,0,0},1,WHITE);
        DrawModelEx(character,skater.position,{0,1,0},skater.heading*RAD2DEG,{1,1,1},WHITE);
        if (showCollision) {
            for(const auto& t:world.triangles) {
                Vector3 center=Vector3Scale(Vector3Add(Vector3Add(t.a,t.b),t.c),1.0f/3);
                if (horizontalDistance(center,skater.position)<500) {
                    DrawLine3D(t.a,t.b,GREEN);DrawLine3D(t.b,t.c,GREEN);DrawLine3D(t.c,t.a,GREEN);
                }
            }
        }
        rlEnableBackfaceCulling();EndMode3D();
        DrawRectangle(12,12,std::min(GetScreenWidth()-24,850),102,{12,18,24,225});
        DrawText("TESTE NATIVO EXPERIMENTAL - HUB",24,22,23,RAYWHITE);
        DrawText("W acelera | A/D viram | S freia | segure/solte ESPACO para ollie",24,53,17,RAYWHITE);
        DrawText("Controle: analogico/direcional, A para ollie | R/Back reposiciona | F3 colisao",24,77,17,RAYWHITE);
        DrawText(TextFormat("%.0f in/s | %s | %s | %d FPS",skater.speed,skater.grounded?"chao":"ar",
            input.gamepad?GetGamepadName(input.gamepadIndex):"teclado",GetFPS()),24,132,19,DARKBLUE);
        DrawText("Fisica inicial e personagem rigido. Manobras, missoes, audio e menus em desenvolvimento.",
                 16,GetScreenHeight()-28,17,BLACK);
        EndDrawing();
        ++frames;
        if(opt.smoke && frames>=opt.frames) break;
    }
    bool passed=!invalid && frames>=opt.frames && travelled>200 && airFrames>0 && landings>0;
    if(opt.smoke) {
        TakeScreenshot("native-smoke.png");
        writeSmoke(passed,skater,travelled,airFrames,landings,frames,static_cast<int>(world.triangles.size()),elapsed,
                   passed?"native integration smoke passed":"native integration incomplete or failed");
    }
    std::cout << "NATIVE: frames=" << frames << " travel=" << travelled << " air=" << airFrames
              << " landings=" << landings << " gamepadObserved=" << sawGamepad << '\n';
    UnloadModel(character);UnloadModel(level);CloseWindow();
    return opt.smoke && !passed?6:0;
}
