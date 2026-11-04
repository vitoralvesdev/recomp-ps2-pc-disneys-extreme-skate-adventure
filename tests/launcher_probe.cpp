// End-to-end packaging probe. It never runs game code.
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
namespace fs=std::filesystem;
static std::wstring env(const wchar_t* key){wchar_t value[32768]{};GetEnvironmentVariableW(key,value,32768);return value;}
int wmain(int argc,wchar_t** argv) {
    const auto root=fs::current_path();
    bool valid=argc==2 && fs::path(argv[1]).is_relative() && fs::absolute(fs::path(argv[1]))==root/L"original"/L"extracted"/L"SLUS_206.07";
    valid=valid && env(L"DISNEY_RENDERER")==L"gpu" && env(L"DISNEY_NATIVE_VBLANK")==L"1" && env(L"PS2X_GS_HW_SCALE")==L"1" && env(L"PS2X_GS_DIRECT_PRESENT")==L"0";
    for(const wchar_t* key:{L"DISNEY_GPU_VERIFY_SECONDS",L"DISNEY_AOT_TEST_FRAMES",L"DISNEY_AOT_TRACE_VU",L"DISNEY_AOT_HOST_CLOCK"})valid=valid && env(key).empty();
    std::ofstream(root/L"launcher-probe-result.txt")<<(valid?"PASS: Unicode paths, working directory, original data argument, GPU configuration and test-limit removal\n":"FAIL: launch configuration\n");
    return valid?0:1;
}
