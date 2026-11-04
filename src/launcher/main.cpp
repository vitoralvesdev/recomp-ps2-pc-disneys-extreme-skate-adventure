#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <string>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace fs=std::filesystem;
static constexpr wchar_t title[]=L"Disney's Extreme Skate Adventure";
struct ChildWindowTitle { DWORD processId; bool changed=false; };
static BOOL CALLBACK setChildWindowTitle(HWND window, LPARAM data) {
    auto* child=reinterpret_cast<ChildWindowTitle*>(data);DWORD processId=0;
    GetWindowThreadProcessId(window,&processId);
    if(processId!=child->processId || !IsWindowVisible(window))return TRUE;
    SetWindowTextW(window,title);child->changed=true;return FALSE;
}
static void nameGameWindow(DWORD processId) {
    for(unsigned attempt=0;attempt<150;++attempt) {
        ChildWindowTitle child{processId};EnumWindows(setChildWindowTitle,reinterpret_cast<LPARAM>(&child));
        if(child.changed)return;Sleep(100);
    }
}
static void output(const std::wstring& value) {
    const auto count=WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    std::string utf8(count,'\0');WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),utf8.data(),count,nullptr,nullptr);
    DWORD written=0;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),utf8.data(),static_cast<DWORD>(utf8.size()),&written,nullptr);
}
static void setLaunchEnvironment(const fs::path &root,bool diagnostico) {
    SetEnvironmentVariableW(L"DISNEY_RENDERER",L"gpu");
    SetEnvironmentVariableW(L"DISNEY_NATIVE_VBLANK",L"1");
    SetEnvironmentVariableW(L"PS2X_GS_HW_SCALE",L"1");
    SetEnvironmentVariableW(L"PS2X_GS_DIRECT_PRESENT",L"0");
    for(const wchar_t* name:{L"DISNEY_HANG_DIAGNOSTICS",L"DISNEY_HANG_OUTPUT",L"DISNEY_DIAG_MENU_INPUT",L"DISNEY_GPU_CAPTURE",L"DISNEY_GPU_VERIFY_SECONDS",L"DISNEY_AOT_TEST_FRAMES",L"DISNEY_PORT_DIAGNOSTICS",L"DISNEY_AOT_PROFILE",L"DISNEY_AOT_TRACE_VU",L"DISNEY_AOT_TRACE_CHAIN",L"DISNEY_AOT_TRACE_KICK",L"DISNEY_AOT_TRACE_PAD",L"DISNEY_AOT_HOST_CLOCK",L"PS2X_VU1_TRACE",L"PS2X_VU1_CAPTURE"})SetEnvironmentVariableW(name,nullptr);
    // "--diagnostico": a play session that leaves evidence when the guest stops making GPU progress (analysis\hang-reports)
    if(diagnostico) {
        const fs::path reports=root/L"analysis"/L"hang-reports";
        std::error_code ignored;fs::create_directories(reports,ignored);
        SetEnvironmentVariableW(L"DISNEY_HANG_DIAGNOSTICS",L"1");
        SetEnvironmentVariableW(L"DISNEY_HANG_OUTPUT",reports.wstring().c_str());
    }
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    bool verify=false,diagnostico=false;
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    for(int i=1;argv && i<argc;++i) {
        const std::wstring argument(argv[i]);
        if(argument==L"--verificar")verify=true;
        else if(argument==L"--diagnostico")diagnostico=true;
    }
    if(argv)LocalFree(argv);
    try {
        std::wstring self(32768,L'\0');const auto length=GetModuleFileNameW(nullptr,self.data(),static_cast<DWORD>(self.size()));
        if(!length || length>=self.size())throw std::runtime_error("module path");self.resize(length);
        const auto root=fs::path(self).parent_path();
        const auto engine=root/L"runtime"/L"DisneySkateRuntime.exe";
        const auto original=root/L"original"/L"extracted"/L"SLUS_206.07";
        std::vector<fs::path> required={root/L"assets"/L"native"/L"audio"/L"catalog.tsv",engine,original,original.parent_path()/L"SKATE4.HED",original.parent_path()/L"SKATE4.WAD"};
        for(const wchar_t* file:{L"SIO2MAN.IRX",L"PADMAN.IRX",L"LIBSD.IRX",L"SDRDRV.IRX",L"MCMAN.IRX",L"MCSERV.IRX",L"EZPCM.IRX"})required.push_back(original.parent_path()/L"IOP"/file);
        for(const wchar_t* dll:{L"avcodec-61.dll",L"avformat-61.dll",L"avutil-59.dll",L"swresample-5.dll",L"swscale-8.dll"})required.push_back(engine.parent_path()/dll);
        for(const auto& path:required)if(!fs::is_regular_file(path)) {
            const auto message=L"Arquivo necessario nao encontrado:\n"+path.wstring()+L"\n\nMantenha este executavel junto das pastas runtime, original e assets.";
            if(verify)output(message+L"\n");else MessageBoxW(nullptr,message.c_str(),title,MB_OK|MB_ICONERROR);
            return 2;
        }
        if(verify) {
            output(L"OK: pacote encontrado; renderer=gpu; nativeVblank=1; escala=1; sem limite de teste.\nRuntime: "+engine.wstring()+L"\nDados: "+original.wstring()+L"\n");
            return 0;
        }
        HANDLE mutex=CreateMutexW(nullptr,TRUE,L"Local\\HighSky_DisneySkate_NativeLauncher");
        if(!mutex)return 3;
        if(GetLastError()==ERROR_ALREADY_EXISTS) {
            MessageBoxW(nullptr,L"O jogo ja esta aberto.",title,MB_OK|MB_ICONINFORMATION);CloseHandle(mutex);return 0;
        }
        setLaunchEnvironment(root,diagnostico);
        fs::create_directories(root/L"analysis");
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
        HANDLE log=CreateFileW((root/L"analysis"/L"game-session.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(log==INVALID_HANDLE_VALUE)log=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
        HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdOutput=log;startup.hStdError=log;startup.hStdInput=input;
        PROCESS_INFORMATION process{};
        // The game runtime has a narrow main(). Keep its data argument ASCII;
        // CreateProcessW supplies the Unicode working directory separately.
        std::wstring command=L"\""+engine.wstring()+L"\" \"original/extracted/SLUS_206.07\"";
        const bool started=CreateProcessW(engine.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,root.c_str(),&startup,&process)!=FALSE;
        const auto error=GetLastError();
        if(log!=INVALID_HANDLE_VALUE)CloseHandle(log);if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);
        if(!started) {
            const auto message=L"Nao foi possivel abrir o jogo. Codigo Windows: "+std::to_wstring(error)+L".\nVerifique a pasta runtime.";
            MessageBoxW(nullptr,message.c_str(),title,MB_OK|MB_ICONERROR);CloseHandle(mutex);return 4;
        }
        CloseHandle(process.hThread);nameGameWindow(process.dwProcessId);WaitForSingleObject(process.hProcess,INFINITE);
        DWORD code=0;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);CloseHandle(mutex);
        if(code!=0)MessageBoxW(nullptr,L"O jogo encerrou com erro. Os detalhes estao em analysis\\game-session.log.",title,MB_OK|MB_ICONERROR);
        return static_cast<int>(code);
    }catch(...) {
        if(verify)output(L"ERRO: nao foi possivel verificar o pacote.\n");
        else MessageBoxW(nullptr,L"Nao foi possivel preparar o jogo. Confira se a pasta do projeto esta acessivel.",title,MB_OK|MB_ICONERROR);
        return 5;
    }
}
