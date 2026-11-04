#include "ps2_runtime.h"
std::unique_ptr<GSRasterBackend> makeDisneyNativeGpuBackend();
#include "games_database.h"
#if defined(PS2X_ENABLE_DEBUG_UI) && !defined(PLATFORM_VITA)
#include "ps2_debug_panel.h"
#endif

#ifdef _DEBUG
#include "ps2_log.h"
#endif

#include <iostream>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cstring>
#include <raylib.h>
#include <rlgl.h>
#include <runtime/ee_scheduler.h>
#include <string>
#include <filesystem>
#include <exception>
#include <algorithm>
#include <cstdlib>

#if defined(__ANDROID__)
#include <android/log.h>
#include <unistd.h>
#include <thread>
#include <cstdio>
#include <cstring>
#endif

namespace
{
#if defined(__ANDROID__)
    int g_logcatPipeFds[2]{-1, -1};
    std::thread g_logcatThread;

    void stopLogcatRedirect()
    {
        std::fflush(stdout);
        std::fflush(stderr);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        if (g_logcatPipeFds[1] >= 0)
        {
            close(g_logcatPipeFds[1]);
            g_logcatPipeFds[1] = -1;
        }
        if (g_logcatThread.joinable())
        {
            g_logcatThread.join();
        }
    }

    void redirectStdioToLogcat()
    {
        if (pipe(g_logcatPipeFds) != 0)
        {
            return;
        }

        setvbuf(stdout, nullptr, _IOLBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);
        dup2(g_logcatPipeFds[1], STDOUT_FILENO);
        dup2(g_logcatPipeFds[1], STDERR_FILENO);

        g_logcatThread = std::thread([]()
                                     {
                                         FILE *reader = fdopen(g_logcatPipeFds[0], "r");
                                         if (!reader)
                                         {
                                             return;
                                         }
                                         char line[1024];
                                         while (fgets(line, sizeof(line), reader))
                                         {
                                             size_t len = std::strlen(line);
                                             if (len > 0 && line[len - 1] == '\n')
                                             {
                                                 line[len - 1] = '\0';
                                             }
                                             __android_log_write(ANDROID_LOG_INFO, "ps2x", line);
                                         }
                                         fclose(reader);
                                         g_logcatPipeFds[0] = -1;
                                     });
        if (std::atexit(stopLogcatRedirect) != 0)
        {
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
            close(g_logcatPipeFds[1]);
            g_logcatPipeFds[1] = -1;
            if (g_logcatThread.joinable())
            {
                g_logcatThread.join();
            }
        }
    }
#endif

    void setupTerminateLogger() // to help on release build crashs
    {
        std::set_terminate([]()
                           {
                               std::cerr << "[terminate] unhandled exception" << std::endl;
                               const std::exception_ptr ep = std::current_exception();
                               if (ep)
                               {
                                   try
                                   {
                                       std::rethrow_exception(ep);
                                   }
                                   catch (const std::system_error &e)
                                   {
                                       std::cerr << "[terminate] std::system_error code=" << e.code().value()
                                                 << " category=" << e.code().category().name()
                                                 << " message=" << e.what() << std::endl;
                                   }
                                   catch (const std::exception &e)
                                   {
                                       std::cerr << "[terminate] std::exception: " << e.what() << std::endl;
                                   }
                                   catch (...)
                                   {
                                       std::cerr << "[terminate] non-std exception" << std::endl;
                                   }
                               }
                               std::abort(); });
    }

    std::string normalizeGameId(const std::string &folderName)
    {
        std::string result = folderName;

        size_t underscore = result.find('_');
        if (underscore != std::string::npos)
            result[underscore] = '-';

        size_t dot = result.find('.');
        if (dot != std::string::npos)
            result.erase(dot, 1);

        std::ranges::transform(result, result.begin(), [](unsigned char character)
                               { return static_cast<char>(std::toupper(character)); });

        return result;
    }

    std::filesystem::path getExecutablePath(int argc, char *argv[])
    {
        // Find bundled game data independently of the shortcut working directory.
        if (argc < 2) {
            auto directory = std::filesystem::absolute(argv[0]).parent_path();
            for (int level = 0; level < 5; ++level) {
                auto candidate = directory / "original" / "extracted" / "SLUS_206.07";
                if (std::filesystem::is_regular_file(candidate)) return candidate;
                directory = directory.parent_path();
            }
        }
        if (argc >= 2 && argv[1] && argv[1][0] != '\0')
        {
            std::cout << "Using argv boot path" << std::endl;
            return std::filesystem::path(argv[1]);
        }
#if defined(PS2X_DEFAULT_BOOT_ELF)
        std::cout << "Using default boot file" << std::endl;
        const std::filesystem::path configuredPath = std::filesystem::path(PS2X_DEFAULT_BOOT_ELF);
#if defined(PLATFORM_VITA)
        return configuredPath;
#endif
        if (configuredPath.is_absolute())
        {
            return configuredPath;
        }
        return (std::filesystem::current_path() / configuredPath).lexically_normal();
#else
        throw std::runtime_error("Unable to determine executable path. Pass the guest ELF as argv[1] or define PS2X_DEFAULT_BOOT_ELF.");
#endif
    }
}

void registerDisneyGuestEntries(PS2Runtime& runtime);
void registerDisneyDiagnostics(PS2Runtime& runtime);
void registerDisneyRecoveredFunctions(PS2Runtime& runtime);

int main(int argc, char *argv[])
{
#if defined(__ANDROID__)
    redirectStdioToLogcat();
#endif
    setupTerminateLogger();

    try
    {
        std::filesystem::path pathObj = getExecutablePath(argc, argv);

        std::string filePathStr = pathObj.string();

        // This is a standalone Disney launcher; do not expose the recompilation
        // tool or the guest executable name in the player-facing window title.
        std::string windowTitle = "Disney's Extreme Skate Adventure";

        if(std::getenv("DISNEY_HANG_DIAGNOSTICS"))windowTitle+=" | DIAGNOSTICO";
        PS2Runtime runtime;
#if defined(PS2X_ENABLE_DEBUG_UI) && !defined(PLATFORM_VITA)
        // This hook is to prevent leak rlimgui deps to recompiler etc
        PS2DebugPanel debugPanel;
        runtime.setDebugUiCallbacks(
            [](PS2Runtime &rt, void *userData)
            {
                (void)rt;
                static_cast<PS2DebugPanel *>(userData)->initialize();
            },
            [](PS2Runtime &rt, void *userData)
            {
                static_cast<PS2DebugPanel *>(userData)->draw(rt);
            },
            [](PS2Runtime &rt, void *userData)
            {
                (void)rt;
                static_cast<PS2DebugPanel *>(userData)->shutdown();
            },
            &debugPanel);
#endif
        if (std::getenv("DISNEY_PORT_DIAGNOSTICS")) {
            runtime.setDebugUiCallbacks(
                [](PS2Runtime&, void*) {},
                [](PS2Runtime& rt, void*) {
                    static unsigned frames = 0;
                    ++frames;
                    if (frames % 120 == 0) {
                        const auto state = rt.eeScheduler().snapshot();
                        const auto graphic=rt.gs().getDebugSnapshot();
                        const auto &registers=rt.memory().gs();
                        std::cerr << "[boot-gs] csr=" << std::hex << registers.csr.load() << " imr=" << registers.imr << " busy=" << rt.memory().read32(0x346b34) << " dispfb=" << registers.dispfb1 << "/" << registers.dispfb2 << " pmode=" << registers.pmode << std::dec << " frame=" << graphic.hostPresentationWidth << "x" << graphic.hostPresentationHeight << " src=" << graphic.hostPresentationSourceFbp << std::endl;
                        std::cerr << "[boot-check] hostFrames=" << frames << " runningThread=" << state.runningThreadId << " threads=" << state.threads.size() << " dma=" << rt.memory().dmaStartCount() << " gif=" << rt.memory().gifCopyCount() << " gs=" << rt.memory().gsWriteCount() << " vif=" << rt.memory().vifWriteCount() << std::endl;
                        for (const auto& thread : state.threads) {
                            std::cerr << "[boot-thread] id=" << thread.id << " pc=0x" << std::hex << thread.pc << " ra=0x" << thread.ra << std::dec << " state=" << static_cast<int>(thread.status) << std::endl;
                        }
                        if (frames == 120) TakeScreenshot("analysis/boot-frame-120.png");
                        if (frames % 600 == 0) TakeScreenshot("boot-latest.png");
                    }
                }, nullptr, nullptr);
        }
        // Bounded original-code integration test. Disabled for normal launches.
        if (std::getenv("DISNEY_AOT_TEST_FRAMES")) {
            runtime.setDebugUiCallbacks(
                [](PS2Runtime&, void*) {},
                [](PS2Runtime& rt, void*) {
                    static unsigned frames=0;
                    static const unsigned limit=std::max(1, std::atoi(std::getenv("DISNEY_AOT_TEST_FRAMES")));
                    ++frames;
                    if (frames==1 || frames%60==0 || frames==limit) {
                        const auto state=rt.eeScheduler().snapshot();
                        const auto gs=rt.gs().getDebugSnapshot();
                        const auto &registers=rt.memory().gs();
                        std::cerr << "[native-aot-frame] frame=" << frames << " fps=" << GetFPS()
                            << " busy=" << std::hex << rt.memory().read32(0x346b34)
                            << " csr=" << registers.csr.load() << " dispfb=" << registers.dispfb1 << '/' << registers.dispfb2
                            << " tex0=" << gs.ctx[0].tex0.tbp0 << ',' << static_cast<unsigned>(gs.ctx[0].tex0.tbw) << ',' << static_cast<unsigned>(gs.ctx[0].tex0.psm)
                            << " frame0=" << gs.ctx[0].frame.fbp << ',' << static_cast<unsigned>(gs.ctx[0].frame.fbw) << ',' << static_cast<unsigned>(gs.ctx[0].frame.psm)
                            << " vu1pc=" << rt.vu1().state().pc << std::dec << std::endl;
                        for (const auto &thread:state.threads)
                            std::cerr << "[native-aot-thread] id=" << thread.id << " pc=" << std::hex << thread.pc << std::dec << std::endl;
                    }
                    if (frames==1) { rlDrawRenderBatchActive(); TakeScreenshot("analysis/native-aot-frame-1.png"); }
                    if (frames==60) { rlDrawRenderBatchActive(); TakeScreenshot("analysis/native-aot-frame-60.png"); }
                    if (frames==120 || frames==limit) {
                        const auto &registers=rt.memory().gs();
                        rlDrawRenderBatchActive();
                        TakeScreenshot("analysis/native-aot-frame-120.png");
                        std::ofstream displayFile("analysis/native-aot-display.txt");
                        displayFile << std::hex << "pmode " << registers.pmode << "\nsmode2 " << registers.smode2
                            << "\ndispfb1 " << registers.dispfb1 << "\ndisplay1 " << registers.display1
                            << "\ndispfb2 " << registers.dispfb2 << "\ndisplay2 " << registers.display2
                            << "\ncsr " << registers.csr.load() << "\nimr " << registers.imr
                            << "\nsiglblid " << registers.siglblid << '\n';
                        std::ofstream ramFile("analysis/native-aot.ram",std::ios::binary);
                        ramFile.write(reinterpret_cast<const char*>(rt.memory().getRDRAM()),32u*1024u*1024u);
                        const auto snapshot=rt.gs().getDebugSnapshot();
                        std::ofstream gsFile("analysis/native-aot-gs.bin",std::ios::binary);
                        gsFile.write(reinterpret_cast<const char*>(&snapshot),sizeof(snapshot));
                        std::ofstream vramFile("analysis/native-aot-vram.bin",std::ios::binary);
                        vramFile.write(reinterpret_cast<const char*>(rt.memory().getGSVRAM()),4u*1024u*1024u);
                        const auto &vuState=rt.vu1().state();
                        std::ofstream vuFile("analysis/native-aot-vu1-state.bin",std::ios::binary);
                        vuFile.write(reinterpret_cast<const char*>(&vuState),sizeof(vuState));
                    }
                    if (frames>=limit) {
                        rlDrawRenderBatchActive();
                        TakeScreenshot("analysis/native-aot-last-frame.png");
                        std::cerr << "[native-aot-test] bounded run ended; this is not a gameplay pass" << std::endl;
                        std::cout.flush(); std::cerr.flush(); std::_Exit(75);
                    }
                }, nullptr, nullptr);
        }
        // One bounded verification after an actual renderer change. Does not
        // overwrite retained RAM/VRAM/VU inputs used by the offline benchmark.
        if(std::getenv("DISNEY_GPU_VERIFY_SECONDS")) {
            runtime.setDebugUiCallbacks([](PS2Runtime&,void*){},[](PS2Runtime& rt,void*) {
                static const auto start=std::chrono::steady_clock::now();
                static const double limit=std::max(1,std::atoi(std::getenv("DISNEY_GPU_VERIFY_SECONDS")));
                static double last=0;static uint64_t lastTicks=0;
                const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
                if(seconds-last>=10.0 || seconds>=limit) {
                    const uint64_t ticks=rt.memory().gs().vsyncTick.load();
                    std::cerr<<"[gpu-verification] seconds="<<seconds<<" syncTicksPerSecond="<<(ticks-lastTicks)/(seconds-last)
                        <<" busy="<<std::hex<<rt.memory().read32(0x346b34)<<std::dec<<std::endl;
                    last=seconds;lastTicks=ticks;
                }
                if(seconds>=limit) {
                    rlDrawRenderBatchActive();TakeScreenshot("analysis/native-gpu-verification.png");
                    std::cerr<<"[gpu-verification] finished; unique gameplay FPS and playability not certified by this counter"<<std::endl;
                    std::cout.flush();std::cerr.flush();std::_Exit(75);
                }
            },nullptr,nullptr);
        }
        if (!runtime.initialize(windowTitle.c_str()))
        {
            std::cerr << "Failed to initialize PS2 runtime" << std::endl;
            return 1;
        }

        const auto mappingsPath = pathObj.parent_path().parent_path().parent_path() / "gamecontrollerdb.txt";
        std::ifstream mappingsFile(mappingsPath);
        if (mappingsFile) {
            std::ostringstream mappingText;
            mappingText << mappingsFile.rdbuf();
            std::cout << "[input] custom mappings loaded: " << SetGamepadMappings(mappingText.str().c_str()) << std::endl;
        }
        for (int index = 0; index < 4; ++index) {
            if (IsGamepadAvailable(index)) std::cout << "[input] controller " << index << ": " << GetGamepadName(index) << std::endl;
        }
        std::cout << "[input] keyboard enabled alongside controllers" << std::endl;
        if (!runtime.loadELF(filePathStr))
        {
            std::cerr << "Failed to load ELF file: " << filePathStr << std::endl;
            return 1;
        }

        if(const char* renderer=std::getenv("DISNEY_RENDERER");!renderer || std::strcmp(renderer,"software")!=0) {
            runtime.gs().setDebugHistoryPaused(true);
            runtime.gs().setRasterBackend(makeDisneyNativeGpuBackend());
            std::cout<<"[native-renderer] ordered GPU worker thread (DISNEY_RENDERER=software disables)"<<std::endl;
        }
        registerDisneyRecoveredFunctions(runtime);
        registerDisneyGuestEntries(runtime);
        registerDisneyDiagnostics(runtime);
        runtime.run();

#ifdef _DEBUG
        ps2_log::print_saved_location();
#endif
        std::cout.flush();
        std::cerr.flush();
        std::_Exit(0);
    }
    catch (const std::exception &e)
    {
        std::cerr << "[main] fatal exception: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "[main] fatal exception: unknown" << std::endl;
    }

    std::cout.flush();
    std::cerr.flush();
    std::_Exit(1);
}










